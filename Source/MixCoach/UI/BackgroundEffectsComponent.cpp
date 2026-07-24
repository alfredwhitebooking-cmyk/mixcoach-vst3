#include "BackgroundEffectsComponent.h"
#include "MixCoachTheme.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════
    BackgroundEffectsComponent::BackgroundEffectsComponent()
    {
        setOpaque(false);
        setInterceptsMouseClicks(false, false);
        setAlwaysOnTop(false);

        // Seed random once
        std::srand((unsigned int)juce::Time::getMillisecondCounter());

        initParticles();
        initOrbs();

        startTimerHz(30); // 30fps es suficiente para partículas decorativas
    }


    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa timer cuando el plugin no es visible
    // ═══════════════════════════════════════════════════════════════════════════

    void BackgroundEffectsComponent::visibilityChanged()
    {
        if (isShowing() && !isTimerRunning()) {
            startTimerHz(30);
        }
        // Nunca detener el timer — el constructor ya lo arrancó.
        // Las partículas y orbes decorativos deben seguir animando
        // incluso si el componente no es visible (vuelve a aparecer animado).
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  initParticles — 25 partículas con distribución uniforme + drift sinusoidal
    // ═══════════════════════════════════════════════════════════════════════════
    void BackgroundEffectsComponent::initParticles()
    {
        particles_.reserve(25);

        for (int i = 0; i < 25; ++i) {
            Particle p;
            // Posición inicial aleatoria en toda la pantalla
            p.x = (float)(std::rand() % 1000) / 1000.0f;
            p.y = (float)(std::rand() % 1000) / 1000.0f;
            // Fase inicial aleatoria
            p.phaseX = (float)(std::rand() % 628) / 100.0f;  // 0..6.28
            p.phaseY = (float)(std::rand() % 628) / 100.0f;
            // Velocidad lenta (periodo ~10-30s para un ciclo completo)
            p.speedX = 0.2f + (float)(std::rand() % 100) / 100.0f * 0.3f;
            p.speedY = 0.15f + (float)(std::rand() % 100) / 100.0f * 0.4f;
            // Amplitud de deriva (1-3% de la pantalla)
            p.ampX = 0.01f + (float)(std::rand() % 100) / 100.0f * 0.03f;
            p.ampY = 0.008f + (float)(std::rand() % 100) / 100.0f * 0.025f;
            // Radio pequeño 1-3px
            p.radius = 1.0f + (float)(std::rand() % 20) / 10.0f;
            // Alpha muy sutil (0.06-0.15)
            p.alpha = 0.06f + (float)(std::rand() % 90) / 1000.0f;
            // Color aleatorio de la paleta
            p.colour = randomParticleColour();

            particles_.push_back(p);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  initOrbs — 3 orbes glow con órbitas Lissajous
    //  Cada orb tiene una frecuencia y fase distinta para crear movimiento
    //  orgánico no repetitivo.
    // ═══════════════════════════════════════════════════════════════════════════
    void BackgroundEffectsComponent::initOrbs()
    {
        orbs_.reserve(3);

        // Orb 1 — Púrpura grande (arriba-izquierda)
        {
            GlowOrb o;
            o.baseX = 0.20f; o.baseY = 0.25f;
            o.radius = 150.0f;
            o.orbitRadiusX = 0.12f; o.orbitRadiusY = 0.08f;
            o.phaseX = 0.0f; o.phaseY = 2.1f;  // ~120° de desfase
            o.speedX = 0.08f; o.speedY = 0.12f;
            o.alpha = 0.07f;
            o.colour = juce::Colour(0xFFA855F7); // púrpura
            orbs_.push_back(o);
        }

        // Orb 2 — Cian mediano (abajo-derecha)
        {
            GlowOrb o;
            o.baseX = 0.75f; o.baseY = 0.70f;
            o.radius = 110.0f;
            o.orbitRadiusX = 0.08f; o.orbitRadiusY = 0.10f;
            o.phaseX = 1.57f; o.phaseY = 0.0f;  // 90° de desfase
            o.speedX = 0.10f; o.speedY = 0.06f;
            o.alpha = 0.06f;
            o.colour = juce::Colour(0xFF06B6D4); // cian
            orbs_.push_back(o);
        }

        // Orb 3 — Verde pequeño (centro-desplazado)
        {
            GlowOrb o;
            o.baseX = 0.50f; o.baseY = 0.50f;
            o.radius = 80.0f;
            o.orbitRadiusX = 0.20f; o.orbitRadiusY = 0.15f;
            o.phaseX = 3.14f; o.phaseY = 4.71f;  // 180° y 270°
            o.speedX = 0.05f; o.speedY = 0.09f;
            o.alpha = 0.05f;
            o.colour = juce::Colour(0xFF22D3A7); // verde
            orbs_.push_back(o);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized — Recalcula orbes cuando el tamaño de la ventana cambia
    // ═══════════════════════════════════════════════════════════════════════════
    void BackgroundEffectsComponent::resized()
    {
        // Los orbes y partículas usan coordenadas relativas (0-1)
        // que se escalan en paint(). No necesita recalcular nada aquí.
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  triggerConfetti — Explosión de 60 partículas de celebración
    // ═══════════════════════════════════════════════════════════════════════════
    void BackgroundEffectsComponent::triggerConfetti(int count)
    {
        // Clear previous confetti
        confetti_.clear();
        confetti_.reserve(count);

        float w = (float)getWidth();
        if (w < 10.0f) w = 800.0f; // fallback si el componente no tiene tamaño aún

        for (int i = 0; i < count; ++i) {
            ConfettiParticle cp;
            // Spawn desde la parte superior con distribución horizontal uniforme
            cp.x = (float)(std::rand() % (int)juce::jmax(1.0f, w));
            cp.y = (float)(-(std::rand() % 60)); // spawn por encima de la pantalla (-60 a 0)

            // Velocidad vertical (caída): 80-200 px/s
            cp.vy = 80.0f + (float)(std::rand() % 120);
            // Velocidad horizontal (deriva): -60 a 60 px/s
            cp.vx = (float)(std::rand() % 120) - 60.0f;

            // Rotación: aleatoria con velocidad media
            cp.rotation = (float)(std::rand() % 628) / 100.0f; // 0..6.28 rad
            cp.rotSpeed = (float)(std::rand() % 300 - 150) / 100.0f; // -1.5..1.5 rad/s

            // Tamaño: 4-8px × 2-6px (rectángulos alargados como confeti real)
            cp.w = 4.0f + (float)(std::rand() % 40) / 10.0f;
            cp.h = 2.0f + (float)(std::rand() % 40) / 10.0f;

            // Lifetime: 2-4 segundos
            cp.maxLifetime = 2.0f + (float)(std::rand() % 200) / 100.0f;
            cp.lifetime = 0.0f;
            cp.alpha = 1.0f;

            // Color brillante de celebración
            cp.colour = randomConfettiColour();

            confetti_.push_back(cp);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateConfetti — Avanza posiciones con gravedad + deriva + rotación
    // ═══════════════════════════════════════════════════════════════════════════
    void BackgroundEffectsComponent::updateConfetti(float dt)
    {
        if (confetti_.empty()) return;

        const float gravity = 120.0f;  // px/s² — caída acelerada
        const float wind = 15.0f;       // px/s² — deriva horizontal suave

        bool anyActive = false;

        for (auto& cp : confetti_) {
            cp.lifetime += dt;

            // Calcular fade: hold 50%, then linear fade to 0 over remaining 50%
            float lifeRatio = cp.lifetime / cp.maxLifetime;
            if (lifeRatio >= 1.0f) {
                cp.alpha = 0.0f;
                continue;
            }
            if (lifeRatio > 0.5f) {
                // Fase de fade-out (50-100%)
                float fadeT = (lifeRatio - 0.5f) / 0.5f;
                cp.alpha = 1.0f - fadeT;
            } else {
                cp.alpha = 1.0f;
            }

            anyActive = true;

            // Aplicar gravedad
            cp.vy += gravity * dt;

            // Aplicar viento (deriva sinusoidal para movimiento orgánico)
            cp.vx += wind * std::sin(cp.lifetime * 2.0f + cp.rotation) * dt;

            // Actualizar posición
            cp.x += cp.vx * dt;
            cp.y += cp.vy * dt;

            // Actualizar rotación
            cp.rotation += cp.rotSpeed * dt;
        }

        // Si no hay confeti activo, limpiar vector para ahorrar memoria
        if (!anyActive)
            confetti_.clear();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawConfetti — Renderiza partículas de celebración
    // ═══════════════════════════════════════════════════════════════════════════
    void BackgroundEffectsComponent::drawConfetti(juce::Graphics& g)
    {
        if (confetti_.empty()) return;

        float w = (float)getWidth();
        float h = (float)getHeight();

        for (const auto& cp : confetti_) {
            if (cp.alpha <= 0.01f) continue;

            // Saltar partículas fuera de la pantalla
            if (cp.y > h + 20.0f || cp.x < -20.0f || cp.x > w + 20.0f) continue;

            g.saveState();

            // Mover al centro de la partícula
            g.addTransform(juce::AffineTransform::translation(cp.x, cp.y));
            // Rotar
            g.addTransform(juce::AffineTransform::rotation(cp.rotation));

            // Color con alpha
            g.setColour(cp.colour.withAlpha(cp.alpha));

            // Dibujar como rectángulo (simula un pedazo de confeti)
            float hw = cp.w * 0.5f;
            float hh = cp.h * 0.5f;
            g.fillRect(-hw, -hh, cp.w, cp.h);

            // Borde sutil para darle más definición
            g.setColour(cp.colour.brighter(0.3f).withAlpha(cp.alpha * 0.3f));
            g.drawRect(-hw, -hh, cp.w, cp.h, 0.5f);

            g.restoreState();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  regenerateNoise — Crea una textura de ruido procedimental usando
    //  juce::Image::BitmapData. Escribe píxeles blancos con valor aleatorio
    //  0-255 para simular ruido analógico.
    // ═══════════════════════════════════════════════════════════════════════════
    void BackgroundEffectsComponent::regenerateNoise(int width, int height)
    {
        if (width < 1 || height < 1) return;

        // Reducir resolución para mejor rendimiento: 1/4 de la pantalla
        int noiseW = juce::jmax(32, width / 4);
        int noiseH = juce::jmax(32, height / 4);

        noiseImage_ = juce::Image(juce::Image::ARGB, noiseW, noiseH, true);

        {
            juce::Image::BitmapData bitmap(noiseImage_, juce::Image::BitmapData::writeOnly);

            for (int y = 0; y < noiseH; ++y) {
                for (int x = 0; x < noiseW; ++x) {
                    uint8_t val = (uint8_t)(std::rand() % 256);
                    // Escribir pixel blanco con alpha=val (0-255, alpha muy bajo luego se escala)
                    // Usamos canal R=G=B=255 y controlamos opacidad vía alpha del canvas
                    // Así el ruido se mezcla con el color de g.setColour() en paint
                    uint8_t* pixel = bitmap.getPixelPointer(x, y);
                    // En Windows/MSVC, JUCE almacena ARGB como BGRA en memoria:
                    // pixel[0]=Blue, pixel[1]=Green, pixel[2]=Red, pixel[3]=Alpha
                    // Para ruido blanco: B=G=R=255, A=random
                    pixel[0] = 255;     // Blue
                    pixel[1] = 255;     // Green
                    pixel[2] = 255;     // Red
                    pixel[3] = val;     // Alpha (se escala después en paint con setOpacity)
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — Actualiza posiciones de partículas, orbes y confeti
    // ═══════════════════════════════════════════════════════════════════════════
    void BackgroundEffectsComponent::timerCallback()
    {
        const float dt = 1.0f / 30.0f; // ~33ms
        elapsed_ += dt;
        ++noiseFrameCounter_;

        // ─── Actualizar confeti (si hay activo) ──────────────────────────────
        updateConfetti(dt);

        // ─── Actualizar partículas (drift sinusoidal) ────────────────────────
        for (auto& p : particles_) {
            // Movimiento ondulante: base position + sine wave drift
            // No mover la base — solo el offset sinusoidal
            float dx = p.ampX * std::sin(elapsed_ * p.speedX + p.phaseX);
            float dy = p.ampY * std::sin(elapsed_ * p.speedY + p.phaseY);
            // p.x, p.y son la posición base (centro del movimiento)
            // El offset se calcula en paint()
            // Actualizar la base lentamente para deriva aún más sutil
            p.x += 0.0001f * std::sin(elapsed_ * 0.05f + p.phaseX);
            p.y += 0.0001f * std::cos(elapsed_ * 0.07f + p.phaseY);
            // Wrap alrededor de 0-1
            if (p.x < 0.0f) p.x += 1.0f;
            if (p.x > 1.0f) p.x -= 1.0f;
            if (p.y < 0.0f) p.y += 1.0f;
            if (p.y > 1.0f) p.y -= 1.0f;
        }

        // ─── Actualizar orbes (órbita Lissajous lenta) ───────────────────────
        for (auto& o : orbs_) {
            float lx = std::sin(elapsed_ * o.speedX + o.phaseX) * o.orbitRadiusX;
            float ly = std::cos(elapsed_ * o.speedY + o.phaseY) * o.orbitRadiusY;
            o.x = o.baseX + lx;
            o.y = o.baseY + ly;
        }

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Dibuja orbes glow + partículas con alpha compositing
    // ═══════════════════════════════════════════════════════════════════════════
    void BackgroundEffectsComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // Saltar render si la ventana está minimizada o tiene tamaño cero
        if (getWidth() < 10 || getHeight() < 10) return;
        float w = bounds.getWidth();
        float h = bounds.getHeight();

        if (w < 1.0f || h < 1.0f) return;

        // Regenerar ruido en paint si es necesario (por si el timer no alcanzó)
        if (!noiseImage_.isValid() || noiseFrameCounter_ % kNoiseRegenInterval == 0) {
            regenerateNoise((int)w, (int)h);
        }

        // ═══ 1. GLOW ORBS (orden: grande → pequeño, para profundidad) ═════════
        // Dibujar primero los orbes más grandes (atrás) y luego los más
        // pequeños (adelante) para crear sensación de profundidad.
        // Orden: púrpura → cian → verde (de atrás a adelante)

        for (size_t i = 0; i < orbs_.size(); ++i) {
            const auto& o = orbs_[i];
            float cx = o.x * w;
            float cy = o.y * h;
            float r = o.radius;

            // Crear gradiente radial desde el centro del orb
            juce::Colour innerCol = o.colour.withAlpha(o.alpha * 1.2f);
            juce::Colour midCol = o.colour.withAlpha(o.alpha * 0.4f);
            juce::Colour outerCol = o.colour.withAlpha(0.0f);

            juce::ColourGradient grad(innerCol, cx, cy,
                                       outerCol, cx + r * 1.5f, cy + r * 1.5f, true);
            grad.addColour(0.4f, midCol);
            grad.addColour(0.8f, outerCol);

            g.setGradientFill(grad);
            g.fillEllipse(cx - r * 1.5f, cy - r * 1.5f,
                          r * 3.0f, r * 3.0f);
        }

        // ═══ 2. PARTICLES ═════════════════════════════════════════════════════
        for (const auto& p : particles_) {
            // Calcular posición con offset sinusoidal
            float dx = p.ampX * std::sin(elapsed_ * p.speedX + p.phaseX);
            float dy = p.ampY * std::sin(elapsed_ * p.speedY + p.phaseY);
            float px = (p.x + dx) * w;
            float py = (p.y + dy) * h;

            // Wrap visual
            if (px < 0.0f) px += w;
            if (px > w) px -= w;
            if (py < 0.0f) py += h;
            if (py > h) py -= h;

            g.setColour(p.colour.withAlpha(p.alpha));
            g.fillEllipse(px - p.radius, py - p.radius,
                          p.radius * 2.0f, p.radius * 2.0f);
        }

        // ═══ 3. CONFETTI CELEBRATION ═════════════════════════════════════════
        // Partículas de celebración que caen con gravedad, deriva y rotación.
        // Se dibujan ANTES del noise para que el ruido las difumine sutilmente.
        drawConfetti(g);
        
        // ═══ 4. NOISE TEXTURE OVERLAY ═════════════════════════════════════════
        // Ruido procedimental regenerado cada kNoiseRegenInterval frames
        // para un efecto flicker sutil analógico.
        if (noiseImage_.isValid()) {
            g.setOpacity(0.03f); // Noise sutil (0.03 = grano analógico como prototipo HTML)
            g.drawImageWithin(noiseImage_,
                              0, 0, (int)w, (int)h,
                              juce::RectanglePlacement::stretchToFit);
            g.setOpacity(1.0f);
        }

        // ═══ 5. GLASS OVERLAY — Borde decorativo con gradiente sutil ══════════
        // GAP #7: Simula el efecto "glass" del HTML usando un borde interior
        // con gradiente semitransparente. Crea la ilusión de un panel de vidrio
        // que envuelve la interfaz.
        {
            // ─── Inner glow: gradiente desde los bordes hacia el centro ──────
            // Usa un rectángulo reducido para el borde interior
            float insetX = w * 0.02f;
            float insetY = h * 0.02f;
            auto glassBounds = bounds.reduced(insetX, insetY);

            // Gradiente vertical: más brillante arriba, más oscuro abajo
            juce::Colour topGlow = juce::Colour(0xFFA855F7).withAlpha(0.025f);    // Púrpura sutil
            juce::Colour midGlow = juce::Colour(0xFF06B6D4).withAlpha(0.015f);    // Cian sutil
            juce::Colour bottomGlow = juce::Colour(0xFF22D3A7).withAlpha(0.010f); // Verde muy sutil

            juce::ColourGradient glassGrad(topGlow, 0.0f, insetY,
                                            bottomGlow, 0.0f, h - insetY, false);
            glassGrad.addColour(0.5f, midGlow);

            g.setGradientFill(glassGrad);
            g.drawRoundedRectangle(glassBounds, 12.0f, 1.0f);

            // ─── Borde exterior más definido (para dar marco) ───────────────
            g.setColour(juce::Colour(0xFFA855F7).withAlpha(0.04f));
            g.drawRoundedRectangle(bounds.reduced(1.0f), 10.0f, 1.5f);
        }
    }

} // namespace mixcoach
