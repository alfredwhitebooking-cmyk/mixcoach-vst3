#!/usr/bin/env python3
"""
code_embedder.py — Code Embedding Pipeline v1.0

CODE EMBEDDING PIPELINE (requirement #2):
Converts files, classes, functions, methods, and modules into semantic embeddings
using pure Python (no external dependencies).

Features:
  - TF-IDF vectorization over code tokens (pure Python implementation)
  - N-gram character hashing for structural similarity (3, 4, 5-gram)
  - File-level embeddings (entire file as bag-of-tokens)
  - Symbol-level embeddings (individual classes/functions)
  - Embedding cache in EMBEDDING_STORE.json
  - Cosine similarity computation
  - Incremental updates (only re-embed changed files)

Usage:
    python code_embedder.py --build-index           # Build/update embedding index
    python code_embedder.py --similarity "query"    # Find similar files
    python code_embedder.py --cache-status          # Show cache status
    python code_embedder.py --file FILE             # Embed single file
"""

import json
import re
import sys
import math
import hashlib
from pathlib import Path
from datetime import datetime
from collections import Counter, defaultdict

# ─── Paths ─────────────────────────────────────────────────────────────────
PROJECT_ROOT = Path(__file__).parent.parent.resolve()

# If that doesn't point to the project root (e.g., script moved), fall back to CWD
if not (PROJECT_ROOT / "Source").exists():
    PROJECT_ROOT = Path.cwd().resolve()  # fallback: run from project root
SOURCE_DIR = PROJECT_ROOT / "Source"
EMBEDDING_PATH = PROJECT_ROOT / "EMBEDDING_STORE.json"
GRAPH_PATH = PROJECT_ROOT / "PROJECT_GRAPH.json"
SYMBOL_GRAPH_PATH = PROJECT_ROOT / "SYMBOL_GRAPH.json"

# ─── Constants ────────────────────────────────────────────────────────────
NGRAM_SIZES = [3, 4, 5]           # Character n-gram sizes for structural hashing
MAX_NGRAM_HASH = 10000            # Hash space for n-gram features
MIN_TOKEN_LENGTH = 2              # Minimum token length to include
MAX_TOKENS_PER_FILE = 5000       # Maximum tokens to extract per file

# TF-IDF smoothing
SMOOTH_IDF = 1.0                  # Additive smoothing for IDF

# C++ stop words — tokens too common across all source files to be useful for search
CPP_STOP_WORDS = {
    # Keywords
    'int', 'void', 'bool', 'float', 'double', 'char', 'auto', 'const',
    'signed', 'unsigned', 'short', 'long', 'static', 'extern', 'register',
    'virtual', 'override', 'final', 'explicit', 'volatile', 'mutable',
    'inline', 'constexpr', 'consteval', 'constinit', 'noexcept', 'nullptr',
    'true', 'false', 'this', 'new', 'delete', 'sizeof', 'typedef', 'using',
    'namespace', 'class', 'struct', 'enum', 'union', 'template', 'typename',
    'public', 'private', 'protected', 'friend', 'operator',
    'begin', 'end', 'size', 'data', 'empty', 'clear', 'push_back', 'emplace_back', 'front', 'back',
    'first', 'second', 'insert', 'erase', 'find', 'count', 'swap',
    # Control flow
    'return', 'if', 'else', 'for', 'while', 'do', 'switch', 'case',
    'break', 'continue', 'default', 'goto', 'throw', 'try', 'catch',
    # Common C++ standard types
    'size_t', 'int8_t', 'int16_t', 'int32_t', 'int64_t',
    'uint8_t', 'uint16_t', 'uint32_t', 'uint64_t',
    'intptr_t', 'uintptr_t', 'ptrdiff_t', 'nullptr_t',
    'string', 'vector', 'array', 'map', 'set', 'pair', 'tuple',
    'optional', 'variant', 'any', 'function', 'shared_ptr',
    'unique_ptr', 'weak_ptr', 'make_shared', 'make_unique',
    # Namespace aliases
    'std', 'juce',
    # Common C++ attributes
    'nodiscard', 'maybe_unused', 'deprecated', 'likely', 'unlikely',
    # Include guard / preprocessor artifacts
    'defined', 'pragma', 'include', 'define', 'ifdef', 'ifndef', 'endif', 'undef',
}

# DSP/Audio specific tokens that get boosted
DSP_BOOST_TOKENS = {
    "fft", "dsp", "audio", "buffer", "sample", "latency", "realtime",
    "lockfree", "thread", "signal", "spectrum", "frequency", "phase",
    "correlation", "amplitude", "waveform", "envelope", "filter",
    "oscillator", "modulation", "gain", "level", "meter", "loudness",
    "lufs", "rms", "peak", "crest", "noise", "distortion",
    "processor", "plugin", "vst3", "midi", "parameter",
    "juce", "audio_processor", "process_block", "prepare_to_play",
}


# ═══════════════════════════════════════════════════════════════════════════
#  Tokenizer — Code-Aware Tokenization
# ═══════════════════════════════════════════════════════════════════════════
class CodeTokenizer:
    """Tokenizes C++ source code into meaningful tokens."""

    @staticmethod
    def _split_camelcase(token):
        """Split a CamelCase or snake_case token into words.
        
        Examples:
            SharedMemory -> ['shared', 'memory']
            SlotRegistry -> ['slot', 'registry']
            TrackDashboardComponent -> ['track', 'dashboard', 'component']
            AudioAnalysis -> ['audio', 'analysis']
            updateSlotColour -> ['update', 'slot', 'colour']
            kMaxSlots -> ['k', 'max', 'slots']  # k prefix is preserved
        """
        # First, split on underscore boundaries
        parts = token.split('_')
        result = []
        for part in parts:
            if not part:
                continue
            # Split CamelCase: insert separator before uppercase following lowercase
            # e.g., 'SharedMemory' -> ['Shared', 'Memory']
            # e.g., 'updateSlot' -> ['update', 'Slot']
            # e.g., 'AudioAnalysis' -> ['Audio', 'Analysis']
            # Handle consecutive uppercase (acronyms): 'VST3' -> stay as is
            #              'HTTPResponse' -> ['HTTP', 'Response']
            split = re.sub(r'(?<=[a-z])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])',
                           ' ', part).split()
            for s in split:
                word = s.lower()
                if len(word) >= 2:  # Keep words of 2+ chars
                    result.append(word)
                elif len(s) == 1 and s.isalpha():
                    # Single letters (common in Hungarian notation like kMaxSlots)
                    if len(result) > 0:  # Don't include isolated single letters
                        result[-1] = s.lower() + result[-1]  # Attach: 'k' + 'max' -> 'kmax'
        return result

    @staticmethod
    def tokenize(content):
        """Tokenize C++ code. Returns list of tokens."""
        # Remove comments
        content = re.sub(r'//.*', '', content)
        content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)

        # Remove string literals
        content = re.sub(r'"[^"]*"', '', content)
        content = re.sub(r"'[^']*'", '', content)

        # Remove preprocessor directives (but keep include paths)
        content = re.sub(r'#\s*\w+\s+[^"\n]*', '', content)
        content = re.sub(r'#\s*\w+', '', content)

        # Tokenize on non-alphanumeric boundaries
        tokens = re.findall(r'[A-Za-z_]\w*', content)

        # Split CamelCase/snake_case into individual words
        expanded = []
        for token in tokens:
            expanded.extend(CodeTokenizer._split_camelcase(token))

        # Filter: remove stop words and short tokens
        tokens = [t for t in expanded
                  if len(t) >= MIN_TOKEN_LENGTH
                  and t not in CPP_STOP_WORDS]

        return tokens

    @staticmethod
    def get_ngrams(text, sizes=None):
        """Generate character n-grams from text."""
        if sizes is None:
            sizes = NGRAM_SIZES
        ngrams = []
        for size in sizes:
            for i in range(len(text) - size + 1):
                ngram = text[i:i + size]
                # Hash to fixed range
                hash_val = int(hashlib.md5(ngram.encode()).hexdigest()[:8], 16) % MAX_NGRAM_HASH
                ngrams.append(hash_val)
        return ngrams

    @staticmethod
    def extract_symbols(content):
        """Extract symbol names (classes, methods, enums) from code."""
        symbols = []
        # Class names
        symbols.extend(re.findall(r'(?:class|struct)\s+(\w+)', content))
        # Method declarations
        symbols.extend(re.findall(r'(?:void|bool|int|float|double|char|auto|juce::\w+|std::\w+)\s+(\w+)\s*\(', content))
        # Enum names
        symbols.extend(re.findall(r'enum\s+(?:class\s+)?(\w+)', content))
        # Namespace names
        symbols.extend(re.findall(r'namespace\s+(\w+)', content))
        return list(set(symbols))


# ═══════════════════════════════════════════════════════════════════════════
#  TF-IDF Vectorizer (Pure Python)
# ═══════════════════════════════════════════════════════════════════════════
class TfIdfVectorizer:
    """TF-IDF vectorizer implemented with pure Python dictionaries."""

    def __init__(self):
        self.document_count = 0
        self.doc_frequencies = Counter()
        self.idf_cache = {}
        self.vocabulary = {}
        self.vocab_size = 0

    def fit(self, tokenized_docs):
        """Fit the vectorizer on a list of token lists."""
        self.document_count = len(tokenized_docs)
        self.doc_frequencies = Counter()

        for tokens in tokenized_docs:
            unique_tokens = set(tokens)
            for token in unique_tokens:
                self.doc_frequencies[token] += 1

        # Build vocabulary
        for token, freq in self.doc_frequencies.most_common():
            self.vocabulary[token] = self.vocab_size
            self.vocab_size += 1

        # Pre-compute IDF
        for token, freq in self.doc_frequencies.items():
            self.idf_cache[token] = math.log(
                (self.document_count + SMOOTH_IDF) / (freq + SMOOTH_IDF)
            ) + 1.0

        return self

    def transform(self, tokens):
        """Transform a token list to TF-IDF vector (dict of dim -> weight)."""
        if not tokens:
            return {}

        token_counts = Counter(tokens)
        max_freq = max(token_counts.values())

        vector = {}
        for token, count in token_counts.items():
            if token in self.vocabulary:
                tf = count / max_freq
                idf = self.idf_cache.get(token, 1.0)
                dim = self.vocabulary[token]
                vector[dim] = tf * idf

        return vector

    def dot_product(self, vec1, vec2):
        """Compute dot product between two sparse vectors."""
        result = 0.0
        for dim, val in vec1.items():
            if dim in vec2:
                result += val * vec2[dim]
        return result

    def magnitude(self, vector):
        """Compute magnitude of a sparse vector."""
        return math.sqrt(sum(v * v for v in vector.values()))

    def cosine_similarity(self, vec1, vec2):
        """Compute cosine similarity between two sparse vectors."""
        dot = self.dot_product(vec1, vec2)
        mag1 = self.magnitude(vec1)
        mag2 = self.magnitude(vec2)
        if mag1 == 0.0 or mag2 == 0.0:
            return 0.0
        return dot / (mag1 * mag2)


# ═══════════════════════════════════════════════════════════════════════════
#  EmbeddingStore — Manages All Embeddings
# ═══════════════════════════════════════════════════════════════════════════
class EmbeddingStore:
    """Manages code embeddings with caching and incremental updates."""

    def __init__(self):
        self.tfidf = TfIdfVectorizer()
        self.documents = {}        # name -> {tokens, ngrams, symbols, embedding, meta}
        self.file_hashes = {}      # file_path -> content_hash
        self.ready = False
        self._load_cache()

    def _load_cache(self):
        """Load cached embeddings from disk."""
        try:
            with open(EMBEDDING_PATH, "r", encoding="utf-8") as f:
                data = json.load(f)
            self.file_hashes = data.get("file_hashes", {})
            self.documents = data.get("documents", {})
            self.ready = True
        except (FileNotFoundError, json.JSONDecodeError):
            self.ready = False

    def _save_cache(self):
        """Save embeddings to disk."""
        data = {
            "version": "2.0.0",
            "generated": datetime.now().isoformat(),
            "total_documents": len(self.documents),
            "file_hashes": self.file_hashes,
            "documents": self.documents,
            "vocabulary_size": self.tfidf.vocab_size,
        }
        with open(EMBEDDING_PATH, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
        print(f"[EMBEDDINGS] Saved: {EMBEDDING_PATH}")
        print(f"  Documents: {len(self.documents)}")

    @staticmethod
    def _content_hash(content):
        """Compute a hash of file content for change detection."""
        return hashlib.md5(content.encode()).hexdigest()[:16]

    def needs_update(self, file_path):
        """Check if a file needs to be re-embedded."""
        full_path = PROJECT_ROOT / file_path
        if not full_path.exists():
            return False
        try:
            content = full_path.read_text(encoding="utf-8")
            current_hash = self._content_hash(content)
            cached_hash = self.file_hashes.get(file_path)
            return current_hash != cached_hash
        except Exception:
            return True

    def embed_file(self, file_path):
        """Generate embeddings for a single file."""
        full_path = PROJECT_ROOT / file_path
        if not full_path.exists():
            print(f"  [WARN] File not found: {file_path}")
            return None

        try:
            content = full_path.read_text(encoding="utf-8")
        except Exception as e:
            print(f"  [WARN] Cannot read {file_path}: {e}")
            return None

        content_hash = self._content_hash(content)

        # Tokenize
        tokens = CodeTokenizer.tokenize(content)

        # Get n-grams
        ngrams = CodeTokenizer.get_ngrams(content)

        # Extract symbols
        symbols = CodeTokenizer.extract_symbols(content)

        # Compute n-gram signature (frequency of each hash value)
        ngram_sig = Counter(ngrams)

        # Boost DSP tokens
        boosted = [t for t in tokens if t.lower() in DSP_BOOST_TOKENS]

        # Create document entry
        doc = {
            "path": file_path,
            "tokens_count": len(tokens),
            "symbols": symbols,
            "ngram_signature": dict(ngram_sig.most_common(100)),
            "dsp_tokens": boosted[:50],
            "content_hash": content_hash,
            "updated": datetime.now().isoformat(),
            "embedding": None,  # Will be computed during fit
        }

        self.documents[file_path] = doc
        self.file_hashes[file_path] = content_hash
        return doc

    def build_index(self, force=False):
        """Build/update the full embedding index."""
        print("[EMBEDDINGS] Building index...")

        # When force=True, clear stale entries first to avoid accumulation
        if force:
            self.documents.clear()
            self.file_hashes.clear()

        # Collect source files
        source_files = []
        source_dir = SOURCE_DIR
        if source_dir.exists():
            for ext in ("*.h", "*.cpp"):
                for f in sorted(source_dir.rglob(ext)):
                    relative = str(f.relative_to(PROJECT_ROOT)).replace("\\", "/")
                    source_files.append(relative)

        # Also include AI config files
        for extra in ["CMakeLists.txt", "build.ps1", "DeployVST3.ps1"]:
            if (PROJECT_ROOT / extra).exists():
                source_files.append(extra)

        # Check which files need updating
        changed_files = []
        for f in source_files:
            if force or self.needs_update(f):
                changed_files.append(f)

        if not changed_files:
            print("[EMBEDDINGS] All files up to date.")
            return

        print(f"  Files to embed: {len(changed_files)}")

        # Embed changed files
        for f in changed_files:
            self.embed_file(f)

        # Collect all tokens for TF-IDF fitting
        all_tokens = []
        valid_docs = []
        for path, doc in self.documents.items():
            full_path = PROJECT_ROOT / path
            if full_path.exists():
                try:
                    content = full_path.read_text(encoding="utf-8")
                    tokens = CodeTokenizer.tokenize(content)
                    all_tokens.append(tokens)
                    valid_docs.append(path)
                except Exception:
                    continue

        if not all_tokens:
            print("[EMBEDDINGS] No documents to index.")
            return

        # Fit TF-IDF
        self.tfidf.fit(all_tokens)

        # Compute embeddings for all valid documents
        for i, path in enumerate(valid_docs):
            tokens = all_tokens[i]
            embedding = self.tfidf.transform(tokens)
            if path in self.documents:
                # Convert to serializable format (string keys)
                self.documents[path]["embedding"] = {str(k): v for k, v in embedding.items()}

        self.ready = True
        self._save_cache()
        print(f"[EMBEDDINGS] Index built: {len(valid_docs)} documents, "
              f"{self.tfidf.vocab_size} vocabulary")

    def search(self, query, top_k=10):
        """Search for documents similar to a text query."""
        if not self.ready or not self.documents:
            self._load_cache()
            if not self.documents:
                print("[EMBEDDINGS] No index available. Run --build-index first.")
                return []

        # Ensure TF-IDF is fitted
        if self.tfidf.vocab_size == 0:
            all_tokens = []
            for path, doc in self.documents.items():
                full_path = PROJECT_ROOT / path
                if full_path.exists():
                    try:
                        content = full_path.read_text(encoding="utf-8")
                        tokens = CodeTokenizer.tokenize(content)
                        all_tokens.append(tokens)
                    except Exception:
                        continue
            if all_tokens:
                self.tfidf.fit(all_tokens)

        # Tokenize query
        query_tokens = CodeTokenizer.tokenize(query)
        query_embedding = self.tfidf.transform(query_tokens)

        if not query_embedding:
            return []

        # Compute similarities
        results = []
        for path, doc in self.documents.items():
            doc_embedding = doc.get("embedding")
            if not doc_embedding:
                continue

            # Convert string keys back to int
            doc_vec = {int(k): v for k, v in doc_embedding.items()}
            similarity = self.tfidf.cosine_similarity(query_embedding, doc_vec)

            # Boost for n-gram overlap with query
            query_ngrams = set(CodeTokenizer.get_ngrams(query))
            doc_ngrams = set(doc.get("ngram_signature", {}).keys())
            ngram_overlap = len(query_ngrams & doc_ngrams) / max(len(query_ngrams), 1)

            # Boost for symbol matches
            query_symbols = set(CodeTokenizer.extract_symbols(query))
            doc_symbols = set(doc.get("symbols", []))
            symbol_overlap = len(query_symbols & doc_symbols) / max(len(query_symbols), 1)

            # Combined score
            combined = similarity * 0.5 + ngram_overlap * 0.3 + symbol_overlap * 0.2

            results.append({
                "path": path,
                "similarity": round(similarity, 4),
                "ngram_match": round(ngram_overlap, 4),
                "symbol_match": round(symbol_overlap, 4),
                "combined_score": round(combined, 4),
                "tokens": doc.get("tokens_count", 0),
                "symbols": doc.get("symbols", [])[:5],
            })

        # Sort by combined score
        results.sort(key=lambda x: x["combined_score"], reverse=True)
        return results[:top_k]

    def get_cache_status(self):
        """Show embedding cache status."""
        print("\n[EMBEDDINGS] Cache Status:")
        print(f"  Documents: {len(self.documents)}")
        print(f"  Vocabulary: {self.tfidf.vocab_size}")
        print(f"  Cache file: {EMBEDDING_PATH}")

        # Document types
        headers = sum(1 for d in self.documents if d.endswith(".h"))
        sources = sum(1 for d in self.documents if d.endswith(".cpp"))
        others = len(self.documents) - headers - sources
        print(f"  Headers: {headers}, Sources: {sources}, Other: {others}")

        # Unchanged files
        unchanged = 0
        for path in self.documents:
            full_path = PROJECT_ROOT / path
            if full_path.exists():
                try:
                    content = full_path.read_text(encoding="utf-8")
                    if self.file_hashes.get(path) == self._content_hash(content):
                        unchanged += 1
                except Exception:
                    pass
        print(f"  Up-to-date: {unchanged}/{len(self.documents)}")

        return {
            "documents": len(self.documents),
            "vocabulary": self.tfidf.vocab_size,
        }


# ═══════════════════════════════════════════════════════════════════════════
#  Main
# ═══════════════════════════════════════════════════════════════════════════
def main():
    import argparse

    parser = argparse.ArgumentParser(description="Code Embedding Pipeline for MixCoach")
    parser.add_argument("--build-index", action="store_true", help="Build/update embedding index")
    parser.add_argument("--force", action="store_true", help="Force rebuild all embeddings")
    parser.add_argument("--similarity", metavar='"QUERY"', help="Search for similar files by query")
    parser.add_argument("--file", metavar="FILE", help="Embed a single file")
    parser.add_argument("--cache-status", action="store_true", help="Show embedding cache status")
    parser.add_argument("--top-k", type=int, default=10, help="Number of results for search")
    args = parser.parse_args()

    if not any(vars(args).values()):
        parser.print_help()
        sys.exit(1)

    store = EmbeddingStore()

    if args.build_index:
        store.build_index(force=args.force)

    if args.similarity:
        results = store.search(args.similarity, top_k=args.top_k)
        if results:
            print(f"\n[SEARCH] Top {len(results)} for: '{args.similarity}'")
            print(f"{'Score':<8} {'Ngram':<8} {'Sym':<8} {'Tokens':<7} File")
            print("-" * 70)
            for r in results:
                print(f"{r['combined_score']:<8.3f} {r['ngram_match']:<8.3f} "
                      f"{r['symbol_match']:<8.3f} {r['tokens']:<7} {r['path']}")
                if r.get('symbols'):
                    print(f"  Symbols: {', '.join(r['symbols'][:3])}")

    if args.file:
        doc = store.embed_file(args.file)
        if doc:
            print(f"\n[EMBED] File: {doc['path']}")
            print(f"  Tokens: {doc['tokens_count']}")
            print(f"  Symbols: {doc['symbols']}")
            print(f"  DSP tokens: {doc['dsp_tokens'][:10]}")

    if args.cache_status:
        store.get_cache_status()


if __name__ == "__main__":
    main()
