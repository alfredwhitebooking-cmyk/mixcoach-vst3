from langchain_ollama import OllamaLLM
from langchain_community.document_loaders import DirectoryLoader
from llama_index.core import VectorStoreIndex, SimpleDirectoryReader

# 1. Conectar Qwen 7B desde Ollama
llm = OllamaLLM(model="qwen2.5:7b")

# 2. Cargar todos los archivos de la carpeta Source
documents = SimpleDirectoryReader("Source").load_data()

# 3. Crear índice con tus fuentes
index = VectorStoreIndex.from_documents(documents)

# 4. Crear un motor de consulta
query_engine = index.as_query_engine(llm=llm)

# 5. Preguntar al modelo con contexto real
response = query_engine.query("Explica cómo se inicializa Messenger y cómo se comunica con Brain")

print(response)


