#pragma once
#include <FastLED.h>
#define RGB_COUNT 89
#define STATIC_COLOR 1
#define BLINK 2
#define PALETTE 3
#define FIRE_2D 4
#define SUNRISE 5

// ==========================================
// EIGENE FARBPALETTEN FÜR REALISTISCHERES FEUER
// ==========================================
DEFINE_GRADIENT_PALETTE( BetterFire_gp ) {
  0,     0,   0,   0,   
  50,  255,   0,   0,   
  120, 255, 100,   0,   
  190, 255, 200,   0,   
  255, 255, 255, 150    
};

DEFINE_GRADIENT_PALETTE( BetterLava_gp ) {
  0,     0,   0,   0,   
  80,  150,   0,   0,   
  150, 220,   0,   0,   
  200, 255,  80,   0,   
  255, 255, 180,   0    
};

// ==========================================
// SONNENAUFGANGS PALETTEN (Laut Research Report Kapitel 6)
// ==========================================

// Modell A: Klarer Horizont (Harte, kräftige Farben)
DEFINE_GRADIENT_PALETTE( Sunrise_Clear_gp ) {
    0,   0,   0,   0,    // Nacht (Schwarz)
   45,   5,   5,  20,    // Astronomisch (Tiefblau)
   90,  20,   0,  51,    // Nautisch (Dunkelviolett)
  140, 102,   0,   0,    // Bürgerlich Start (Tiefrot)
  180, 255,  36,   0,    // Sonnenrand (Leuchtend Rot)
  220, 255, 140,   0,    // Goldene Stunde (Orange)
  255, 255, 255, 255     // Tageslicht (Weiß)
};

// Modell B: Bergnebel (Mie-Streuung, Pastell, diffus)
DEFINE_GRADIENT_PALETTE( Sunrise_Fog_gp ) {
    0,   2,   2,   5,    // Nacht im Nebel
   60,  18,  21,  38,    // Blaue Stunde (diffus)
  115,  59,  34,  76,    // Dunst-Färbung (Lila)
  170, 179,  89,  76,    // Gedämpftes Licht (Altrosa)
  210, 217, 155, 148,    // Aufhellung (Pastell-Rosa/Orange)
  255, 230, 230, 250     // Nebelblendung (Kaltweißes Streulicht)
};

// Modell C: Arktisch (Extrem lange blaue Phase)
DEFINE_GRADIENT_PALETTE( Sunrise_Arctic_gp ) {
    0,   0,   0,   0,
   80,   0,  10,  30,    // Langsame blaue Aufhellung
  150,   0,  40,  80,    // Ausgedehnte nautische Dämmerung
  200,  30,   0,  50,    // Spätes Violett
  230, 150,  30,   0,    // Später, kurzer Rot-Durchbruch
  255, 255, 200, 100     // Flaches, warmes Endlicht
};

typedef struct{
    uint8_t id;
    uint8_t type;
    char name[14];
    uint8_t data[16];
}AnimationSetting;

class IAnimation
{
    public:
        virtual void ResetSettings() = 0;
        virtual void RestartAnimation() = 0;
        virtual bool Update(unsigned long tick) = 0; 
        virtual String GetAvailableSettings()
        {
            return "No Settings Available";
        }
        virtual bool UpdateSetting(int index, unsigned long value)
        {
            return false;
        }
        virtual int GetSetting(int index) = 0;
        virtual String GetName() = 0;
        virtual void getAnimationSetting(AnimationSetting* settings) = 0;
        virtual void applyAnimationSetting(AnimationSetting* settings) = 0;
        virtual ~IAnimation() {}

};

class StaticColorAnimation: public IAnimation
{
    public:
        StaticColorAnimation(struct CRGB *targetArray, int RGBCount)
        {
            leds = targetArray;
            rgb_count = RGBCount;
        }
        void ResetSettings() override
        {
            brightness = 0xFF;
            color = 0xFFFFFF;
            update_needed=true;
        }
        void RestartAnimation() override
        {
            fill_solid(leds, rgb_count, color);
            FastLED.setBrightness(brightness);   
            update_needed=true;
        }
        bool Update(unsigned long tick) override
        {
            if(update_needed) 
            {
                RestartAnimation();
                update_needed=false;
                return true;
            }
            return false;
        }

        bool UpdateSetting(int index, unsigned long value) override
        {
            switch(index)
            {
                case 0:
                    if(value > 0xFFFFFF)return false;
                    color = value;
                    update_needed = true;
                    break;
                
                case 1:
                    if(value > 0xFF)return false;
                    brightness = (uint8_t) value;
                    update_needed = true;
                    break;
                default:
                    return false;
            }
            return true;
        }

        int GetSetting(int index)
        {
            switch(index)
            {
                case 0: return color;
                case 1: return brightness;
                default: return -1;
            }
        }

        String GetAvailableSettings() override
        {
            return "0: Color\n1:Brightness";
        }

        String GetName() override
        {
            return name;
        }

        void getAnimationSetting(AnimationSetting* settings)
        {
            settings->id = id;
            settings->type = STATIC_COLOR;
            
            memset(settings->name, 0, sizeof(settings->name));
            int len = name.length();
            if (len > 13) len = 13;
            memcpy(settings->name, name.c_str(), len);

            settings->data[0] = brightness;
            settings->data[1] = (uint8_t)(color & 0xFF);
            settings->data[2] = (uint8_t)((color >> 8) & 0xFF);
            settings->data[3] = (uint8_t)((color >> 16) & 0xFF);
        }
        
        void applyAnimationSetting(AnimationSetting* settings)
        {
            id = settings->id;
            char tempName[14] = {0};
            strncpy(tempName, settings->name, 13);
            name = String(tempName);
            brightness = settings->data[0];
            color = 0;
            color |= settings->data[1];
            color |= (((unsigned long)settings->data[2]) << 8);
            color |= (((unsigned long)settings->data[3]) << 16);
            update_needed = true;
        }
    
    private:
        uint8_t brightness = 0;
        unsigned long color = 0xFFFFFF;
        String name = "";
        CRGB *leds;
        int rgb_count = 0;
        bool update_needed = false;
        uint8_t id = 0;
};

class BlinkAnimation: public IAnimation
{
    public:
        BlinkAnimation(struct CRGB *targetArray, int RGBCount)
        {
            leds = targetArray;
            rgb_count = RGBCount;
        }
        void ResetSettings() override
        {
            brightness = 0xFF;
            color_on = 0xFFFFFF;
            color_off = 0;
            cycle_ticks = 10;
            update_needed=true;
        }
        void RestartAnimation()
        {
            fill_solid(leds, rgb_count, color_off);
            FastLED.setBrightness(brightness);   
        }
        bool Update(unsigned long tick) override
        {
            if(!((tick+int(cycle_ticks/2))%cycle_ticks))
            {
                fill_solid(leds, RGB_COUNT, CRGB::Red);
                return true;
            }
            else if(!(tick%cycle_ticks))
            {
                fill_solid(leds, RGB_COUNT, CRGB::Black);
                return true;
            }
            return false;
        }

        bool UpdateSetting(int index, unsigned long value) override
        {
            
            switch(index)
            {
                case 0:
                    if(value > 0xFFFFFF)return false;
                    color_on = value;
                    update_needed = true;
                    break;

                case 1:
                    if(value > 0xFFFFFF)return false;
                    color_off = value;
                    update_needed = true;
                    break;

                case 2:
                    if(value > 0xFF)return false;
                    cycle_ticks = value;
                    update_needed = true;
                    break;

                case 3:
                    if(value > 0xFF)return false;
                    brightness = value;
                    update_needed = true;
                    break;
                default:
                    return false;
            } 
            return true;
        }

        int GetSetting(int index)
        {
            switch(index)
            {
                case 0: return color_on;
                case 1: return color_off;
                case 2: return cycle_ticks;
                case 3: return brightness;
                default: return -1;
            }
        }

        String GetAvailableSettings() override
        {
            return "0: Color On\n1: Color Off\n2: Cycle duration in ms/100 \n3:Brightness";
        }

        String GetName() override
        {
            return name;
        }

        void getAnimationSetting(AnimationSetting* settings)
        {
            settings->id = id;
            settings->type = BLINK;

            memset(settings->name, 0, sizeof(settings->name));
            int len = name.length();
            if (len > 13) len = 13;
            memcpy(settings->name, name.c_str(), len);

            settings->data[0] = brightness;
            settings->data[1] = (uint8_t)(color_on & 0xFF);
            settings->data[2] = (uint8_t)((color_on >> 8) & 0xFF);
            settings->data[3] = (uint8_t)((color_on >> 16) & 0xFF);
            settings->data[4] = (uint8_t)(color_off & 0xFF);
            settings->data[5] = (uint8_t)((color_off >> 8) & 0xFF);
            settings->data[6] = (uint8_t)((color_off >> 16) & 0xFF);
            settings->data[7] = (uint8_t)cycle_ticks;
        }
        
        void applyAnimationSetting(AnimationSetting* settings)
        {
            id = settings->id;
            char tempName[14] = {0};
            strncpy(tempName, settings->name, 13);
            name = String(tempName);
            brightness = settings->data[0];
            cycle_ticks = settings->data[7];
            color_on = 0;
            color_on |= settings->data[1];
            color_on |= (((unsigned long)settings->data[2]) << 8);
            color_on |= (((unsigned long)settings->data[3]) << 16);
            color_off = 0;
            color_off |= settings->data[4];
            color_off |= (((unsigned long)settings->data[5]) << 8);
            color_off |= (((unsigned long)settings->data[6]) << 16);
            update_needed = true;
        }
    
    private:
        int id = 0;
        int brightness = 0;
        unsigned long color_on = 0xFFFFFF;
        unsigned long color_off = 0;
        uint8_t cycle_ticks = 10;
        String name = "";
        CRGB *leds;
        int rgb_count = 0;
        bool update_needed = false;
};

class PaletteAnimation : public IAnimation
{
public:
    PaletteAnimation(struct CRGB *targetArray, int RGBCount)
    {
        leds = targetArray;
        rgb_count = RGBCount;
        currentPalette = RainbowColors_p;
    }

    void ResetSettings() override
    {
        brightness = 255;
        speed = 10;
        delta = 3;
        paletteID = 0;
        update_needed = true;
    }

    void RestartAnimation() override
    {
        FastLED.setBrightness(brightness);
        ChangePalette(paletteID);
    }

    bool Update(unsigned long tick) override
    {
        uint8_t startIndex = (uint8_t)((tick * speed) >> 2);
        
        fill_palette(leds, rgb_count, startIndex, delta, currentPalette, 255, LINEARBLEND);

        if(FastLED.getBrightness() != brightness) {
            FastLED.setBrightness(brightness);
        }
        
        return true;
    }

    void ChangePalette(uint8_t id)
    {
        paletteID = id;
        switch(id)
        {
            case 0: currentPalette = RainbowColors_p; break;
            case 1: currentPalette = PartyColors_p; break;
            case 2: currentPalette = OceanColors_p; break;     
            case 3: currentPalette = ForestColors_p; break;    
            case 4: currentPalette = BetterFire_gp; break;
            case 5: currentPalette = BetterLava_gp; break;
            case 6: 
                currentPalette = CRGBPalette16(CRGB::Black, CRGB::Green, CRGB::Black, CRGB::DarkGreen);
                break;
            default: currentPalette = RainbowColors_p; break;
        }
    }

    bool UpdateSetting(int index, unsigned long value) override
    {
        switch(index)
        {
            case 0: 
                if(value > 255) return false;
                ChangePalette((uint8_t)value);
                break;
            case 1: 
                if(value > 255) return false;
                speed = (uint8_t)value;
                break;
            case 2: 
                if(value > 255) return false;
                delta = (uint8_t)value;
                break;
            case 3: 
                if(value > 255) return false;
                brightness = (uint8_t)value;
                break;
            default:
                return false;
        }
        return true;
    }

    int GetSetting(int index) override
    {
        switch(index)
        {
            case 0: return paletteID;
            case 1: return speed;
            case 2: return delta;
            case 3: return brightness;
            default: return -1;
        }
    }

    String GetAvailableSettings() override
    {
        return "0: Palette ID\n1: Speed\n2: Delta\n3: Brightness";
    }

    String GetName() override
    {
        return name;
    }

    void getAnimationSetting(AnimationSetting* settings) override
    {
        settings->id = id;
        settings->type = PALETTE;

        memset(settings->name, 0, sizeof(settings->name));
        int len = name.length();
        if (len > 13) len = 13;
        memcpy(settings->name, name.c_str(), len);

        settings->data[0] = brightness;
        settings->data[1] = paletteID;
        settings->data[2] = speed;
        settings->data[3] = delta;
    }

    void applyAnimationSetting(AnimationSetting* settings) override
    {
        id = settings->id;
        char tempName[14] = {0};
        strncpy(tempName, settings->name, 13);
        name = String(tempName);
        brightness = settings->data[0];
        uint8_t newPalID = settings->data[1];
        speed = settings->data[2];
        delta = settings->data[3];
        
        if(speed == 0) speed = 1;
        ChangePalette(newPalID);
        update_needed = true;
    }

private:
    uint8_t id = 0;
    String name = "";
    CRGB *leds;
    int rgb_count;
    
    CRGBPalette16 currentPalette;
    
    uint8_t brightness;
    uint8_t paletteID;
    uint8_t speed;
    uint8_t delta;
    
    bool update_needed = false;
};

class HorizontalFireAnimation : public IAnimation {
public:
    HorizontalFireAnimation(struct CRGB *targetArray, int RGBCount)
    {
        leds = targetArray;
        rgb_count = RGBCount;
        
        heat = new uint8_t[rgb_count];
        temp_heat = new uint8_t[rgb_count]; 
        
        memset(heat, 0, rgb_count);
        memset(temp_heat, 0, rgb_count);
        
        num_embers = (rgb_count / 18) + 1;
        ember_positions = new uint16_t[num_embers];
        
        for (uint8_t i = 0; i < num_embers; i++) {
            uint16_t sector_size = rgb_count / num_embers;
            uint16_t base_pos = i * sector_size;
            ember_positions[i] = base_pos + random8(0, sector_size / 2);
        }
        
        for (uint8_t i = 0; i < MAX_SPARKS; i++) {
            sparks[i].active = false;
        }

        ChangePalette(4); 
    }

    virtual ~HorizontalFireAnimation() {
        delete[] heat;
        delete[] temp_heat;
        delete[] ember_positions;
    }

    void ResetSettings() override {
        brightness = 255;
        cooling_base = 45;
        sparking_chance = 110;
        paletteID = 4;
        ChangePalette(paletteID);
    }

    void RestartAnimation() override {
        FastLED.setBrightness(brightness);
        memset(heat, 0, rgb_count); 
        for (uint8_t i = 0; i < MAX_SPARKS; i++) {
            sparks[i].active = false;
        }
    }

    bool Update(unsigned long tick) override {
        for (uint16_t i = 0; i < rgb_count; i++) {
            uint8_t random_cooling = random8(0, ((cooling_base * 10) / rgb_count) + 2);
            heat[i] = qsub8(heat[i], random_cooling);
        }
        
        diffuseHeatBidirectional();
        injectEmbers();
        updateSparks();

        if(FastLED.getBrightness() != brightness) {
            FastLED.setBrightness(brightness);
        }

        for (uint16_t i = 0; i < rgb_count; i++) {
            uint8_t colorIndex = scale8(heat[i], 240);
            leds[i] = ColorFromPalette(firePalette, colorIndex);
        }
        
        for (uint8_t i = 0; i < MAX_SPARKS; i++) {
            if (sparks[i].active) {
                uint16_t int_pos = (uint16_t)sparks[i].position;
                float fraction = sparks[i].position - int_pos;
                uint8_t primary_intensity = sparks[i].intensity * (1.0f - fraction);
                uint8_t secondary_intensity = sparks[i].intensity * fraction;
                
                if (int_pos < rgb_count) {
                    leds[int_pos] += CRGB(primary_intensity, primary_intensity, primary_intensity / 2);
                }
                if (int_pos + 1 < rgb_count) {
                    leds[int_pos + 1] += CRGB(secondary_intensity, secondary_intensity, secondary_intensity / 2);
                }
            }
        }

        return true; 
    }

    void ChangePalette(uint8_t id) {
        paletteID = id;
        switch(id) {
            case 0: firePalette = RainbowColors_p; break;
            case 1: firePalette = PartyColors_p; break;
            case 2: firePalette = OceanColors_p; break;     
            case 3: firePalette = ForestColors_p; break;    
            case 4: firePalette = BetterFire_gp; break;     
            case 5: firePalette = BetterLava_gp; break;     
            case 6: firePalette = CRGBPalette16(CRGB::Black, CRGB::Green, CRGB::Black, CRGB::DarkGreen); break;
            default: firePalette = BetterFire_gp; break;
        }
    }

    int GetSetting(int index) override {
        switch(index) {
            case 0: return cooling_base;
            case 1: return sparking_chance;
            case 2: return paletteID;
            case 3: return brightness;
            default: return -1;
        }
    }

    String GetAvailableSettings() override { return "0: Cooling\n1: Sparking\n2: Palette\n3: Brightness"; }
    String GetName() override { return name; }

    void getAnimationSetting(AnimationSetting* settings) override {
        settings->id = id;
        settings->type = FIRE_2D;

        memset(settings->name, 0, sizeof(settings->name));
        int len = name.length();
        if (len > 13) len = 13;
        memcpy(settings->name, name.c_str(), len);

        settings->data[0] = brightness;
        settings->data[1] = cooling_base;
        settings->data[2] = sparking_chance;
        settings->data[3] = paletteID;
    }

    void applyAnimationSetting(AnimationSetting* settings) override {
        id = settings->id;
        char tempName[14] = {0};
        strncpy(tempName, settings->name, 13);
        name = String(tempName);
        
        brightness = settings->data[0];
        cooling_base = settings->data[1];
        sparking_chance = settings->data[2];
        ChangePalette(settings->data[3]);
        
        if(cooling_base == 0) cooling_base = 45; 
    }

private:
    uint8_t id = 0;
    String name = "";
    CRGB *leds;
    int rgb_count;
    
    CRGBPalette16 firePalette;
    uint8_t brightness = 255;
    uint8_t paletteID = 4;

    uint8_t cooling_base = 45;
    uint8_t sparking_chance = 110;

    uint8_t *heat;
    uint8_t *temp_heat;
    uint16_t *ember_positions;
    uint8_t num_embers;

    struct FireSpark {
        bool active;            
        float position;         
        float velocity;         
        uint8_t intensity;      
        uint8_t cooling_rate;   
    };

    static const uint8_t MAX_SPARKS = 20;
    FireSpark sparks[MAX_SPARKS];

    void diffuseHeatBidirectional() {
        for (uint16_t i = 0; i < rgb_count; i++) {
            uint16_t left_heat = (i > 0) ? heat[i - 1] : 0;
            uint16_t right_heat = (i < rgb_count - 1) ? heat[i + 1] : 0;
            uint16_t center_heat = heat[i];
            temp_heat[i] = (center_heat >> 1) + (left_heat >> 2) + (right_heat >> 2);
        }
        memcpy(heat, temp_heat, rgb_count);
    }

    void injectEmbers() {
        bool wind_gust = random8() < 20; 
        
        for (uint8_t i = 0; i < num_embers; i++) {
            uint16_t pos = ember_positions[i];
            uint8_t heat_added = random8(70, 180);
            
            if (wind_gust) {
                heat_added = random8(150, 220); 
            } else if (random8() < 30) {
                heat_added = random8(30, 70);   
            }
            
            heat[pos] = qadd8(heat[pos], heat_added);
            
            if (random8() < sparking_chance) {
                spawnSpark(pos);
            }
        }
    }

    void spawnSpark(uint16_t origin_pos) {
        for (uint8_t i = 0; i < MAX_SPARKS; i++) {
            if (!sparks[i].active) {
                sparks[i].active = true;
                sparks[i].position = (float)origin_pos;
                
                float vel = (random8(0, 240) / 100.0f) - 1.2f;
                if(vel >= 0.0f && vel < 0.35f) vel = 0.4f;
                if(vel < 0.0f && vel > -0.35f) vel = -0.4f;
                
                sparks[i].velocity = vel;
                sparks[i].intensity = random8(210, 255);
                sparks[i].cooling_rate = random8(8, 25);
                break;
            }
        }
    }

    void updateSparks() {
        for (uint8_t i = 0; i < MAX_SPARKS; i++) {
            if (sparks[i].active) {
                sparks[i].position += sparks[i].velocity;
                sparks[i].intensity = qsub8(sparks[i].intensity, sparks[i].cooling_rate);
                sparks[i].velocity *= 0.96f;
                
                if (sparks[i].intensity < 10 || sparks[i].position < 0 || sparks[i].position >= rgb_count) {
                    sparks[i].active = false;
                }
            }
        }
    }
};

// ==========================================
// NEU: Sonnenaufgangs Animation (Basierend auf Research Report)
// ==========================================
class SunriseAnimation : public IAnimation {
public:
    SunriseAnimation(struct CRGB *targetArray, int RGBCount) {
        leds = targetArray;
        rgb_count = RGBCount;
        ChangePalette(0); // Start mit Modell A (Klarer Horizont)
    }

    void ResetSettings() override {
        brightness = 255;
        duration_sec = 120; // 2 Minuten Default
        paletteID = 0;
        shimmer_intensity = 30; // Leichtes Wolkenflackern
        ChangePalette(paletteID);
    }

    void RestartAnimation() override {
        FastLED.setBrightness(brightness);
        start_millis = millis();
        animation_finished = false;
    }

    bool Update(unsigned long tick) override {
        // Nutze Echtzeit (millis) anstelle von Tick-Zählern, um Drift zu vermeiden
        // Siehe Research Report Kapitel 7.3
        unsigned long currentMillis = millis();
        unsigned long durationMs = duration_sec * 1000UL;
        unsigned long elapsedTime = currentMillis - start_millis;

        uint8_t colorIndex = 255; // Default: Ende der Animation (Hell)

        if (elapsedTime < durationMs) {
            // Dynamische Projektion auf 0-255 Palettenindex
            colorIndex = (elapsedTime * 255) / durationMs;
        } else {
            animation_finished = true;
        }

        // Stelle sicher, dass die globale Helligkeit stimmt (ohne Blockade)
        if(FastLED.getBrightness() != brightness) {
            FastLED.setBrightness(brightness);
        }

        // Render Frame mit oder ohne Shimmer-Effekt (Research Report Kapitel 8.2)
        if (shimmer_intensity == 0 || animation_finished) {
            // Einfaches Auffüllen, wenn kein Shimmer gewollt oder Animation zu Ende (Stabilität)
            fill_solid(leds, rgb_count, ColorFromPalette(sunrisePalette, colorIndex, 255, LINEARBLEND));
        } else {
            // Stochastisches Wolkenschimmern berechnen
            for(int i = 0; i < rgb_count; i++) {
                // Perlin Noise: Langsame zeitliche (millis/4) und räumliche (i*30) Veränderung
                uint8_t noise = inoise8(currentMillis / 4, i * 30);
                
                // Je nach shimmer_intensity wird der Noise-Faktor skaliert.
                // Ein hoher Shimmer sorgt für tiefe Helligkeits-Einbrüche (Schatten)
                uint8_t dim_factor = scale8(255 - noise, shimmer_intensity);
                
                // Pixelhelligkeit ist 255 abzüglich dem Dim-Faktor
                uint8_t pixelBrightness = qsub8(255, dim_factor);
                
                // Nutze LINEARBLEND für CIELAB-ähnliche smoothe Übergänge aus der FastLED Palette
                leds[i] = ColorFromPalette(sunrisePalette, colorIndex, pixelBrightness, LINEARBLEND);
            }
        }

        return true; 
    }

    void ChangePalette(uint8_t id) {
        paletteID = id;
        switch(id) {
            case 0: sunrisePalette = Sunrise_Clear_gp; break;
            case 1: sunrisePalette = Sunrise_Fog_gp; break;
            case 2: sunrisePalette = Sunrise_Arctic_gp; break;
            default: sunrisePalette = Sunrise_Clear_gp; break;
        }
    }

    int GetSetting(int index) override {
        switch(index) {
            case 0: return duration_sec;
            case 1: return paletteID;
            case 2: return shimmer_intensity;
            case 3: return brightness;
            default: return -1;
        }
    }

    String GetAvailableSettings() override { 
        return "0: Duration (sec)\n1: Palette Model\n2: Shimmer\n3: Brightness"; 
    }
    
    String GetName() override { return name; }

    void getAnimationSetting(AnimationSetting* settings) override {
        settings->id = id;
        settings->type = SUNRISE;

        memset(settings->name, 0, sizeof(settings->name));
        int len = name.length();
        if (len > 13) len = 13;
        memcpy(settings->name, name.c_str(), len);

        settings->data[0] = brightness;
        settings->data[1] = duration_sec; // Max 255 Sekunden (~4.25 Minuten)
        settings->data[2] = paletteID;
        settings->data[3] = shimmer_intensity;
    }

    void applyAnimationSetting(AnimationSetting* settings) override {
        id = settings->id;
        char tempName[14] = {0};
        strncpy(tempName, settings->name, 13);
        name = String(tempName);
        
        brightness = settings->data[0];
        duration_sec = settings->data[1];
        ChangePalette(settings->data[2]);
        shimmer_intensity = settings->data[3];
        
        if(duration_sec == 0) duration_sec = 120; // Fallback
    }

private:
    uint8_t id = 0;
    String name = "";
    CRGB *leds;
    int rgb_count;
    
    CRGBPalette16 sunrisePalette;
    uint8_t brightness = 255;
    
    // Parameter
    uint8_t duration_sec = 120;
    uint8_t paletteID = 0;
    uint8_t shimmer_intensity = 30;

    // Runtime state
    unsigned long start_millis = 0;
    bool animation_finished = false;
};

// ==========================================
// BCD Clock Overlay
// ==========================================
class BCDClockOverlay {
public:
    BCDClockOverlay(struct CRGB *targetArray, int RGBCount)
    {
        leds = targetArray;
        rgb_count = RGBCount;
    }

    void setEnabled(bool en) { enabled = en; }
    bool isEnabled() { return enabled; }
    
    void setColor(CRGB c) { color = c; }

    void UpdateAndDraw(uint8_t hours, uint8_t minutes, bool blinkTick) {
        if (!enabled || rgb_count < 14) return;

        int start_idx = rgb_count - 14;

        for(int i = 0; i < 14; i++) {
            leds[start_idx + i].nscale8(40); 
        }

        drawBits(start_idx, 2, hours / 10);
        drawBits(start_idx + 2, 4, hours % 10);

        drawBits(start_idx + 6, 3, minutes / 10);
        drawBits(start_idx + 9, 4, minutes % 10);

        if (blinkTick) {
            leds[start_idx + 13] = color;
        }
    }

private:
    CRGB *leds;
    int rgb_count;
    bool enabled = false;
    CRGB color = CRGB::White; 

    void drawBits(int start_idx, int num_bits, uint8_t value) {
        for (int i = 0; i < num_bits; i++) {
            int bit_pos = num_bits - 1 - i;
            if ((value >> bit_pos) & 0x01) {
                leds[start_idx + i] = color;
            }
        }
    }
};

// ==========================================
// AnimationManager
// ==========================================
class AnimationManager
{
    public: 
        AnimationManager(struct CRGB *targetArray_, int RGBCount_, Preferences& storage)
        {
            leds = targetArray_;
            rgb_count = RGBCount_;
        }

        void begin()
        {
            memset(animations, 0, sizeof(animations));
            animation_count = createAnimationsFromStorage();
        }

        ~AnimationManager(){};

        int getAnimationIndex(String name)
        {
            int i = 0;
            while(i < 100)
            {
                if (animations[i] != nullptr && name == animations[i]->GetName())break;
                i++;
            }
            if(i < 100)return i;
            else return -1;
        }

        IAnimation* getAnimation(int index)
        {
            if(index < 0 || index >= 100)return nullptr;
            else return animations[index];
        }

        IAnimation* getAnimationByName(String name)
        {
            int id = getAnimationIndex(name);
            if(id==-1)return nullptr;
            else return animations[id];
        }
        
        int createAnimation(AnimationSetting* settings, bool save)
        {
            if(settings == nullptr)return -1;

            int i = 0;
            while(i<100&&animations[i]!=nullptr) i++;
            if (i>=100) return -2;

            IAnimation* animation = nullptr;
            if(settings->type==STATIC_COLOR)
            {
                animation = new StaticColorAnimation(leds, rgb_count);
            }
            else if(settings->type==BLINK)
            {
                animation = new BlinkAnimation(leds, rgb_count);
            }
            else if(settings->type == PALETTE)
            {
                animation = new PaletteAnimation(leds, rgb_count);
            }
            else if(settings->type == FIRE_2D)
            {
                animation = new HorizontalFireAnimation(leds, rgb_count);
            }
            else if(settings->type == SUNRISE) // NEU: Instanziierung der Sunrise Klasse
            {
                animation = new SunriseAnimation(leds, rgb_count);
            }
            else return -3;
            
            settings->id = i;
            animation->applyAnimationSetting(settings);
            
            if(save)saveAnimation(settings);
            
            animations[i]=animation;
            animation_count++;
            return i;
        }

        int createAnimation(AnimationSetting* settings)
        {
            return createAnimation(settings, false);
        }

        void saveAnimation(AnimationSetting* settings)
        {
            _storage.begin("anim_data");
            String key = "a"+ String(settings->id);
            _storage.putBytes(key.c_str(),settings,sizeof(AnimationSetting));
            _storage.end();
        }

        bool saveAnimationIndex(int id)
        {
            if(id < 0 || id >= 100)return false;
            if (animations[id] == nullptr) return false;
            AnimationSetting settings;
            animations[id]->getAnimationSetting(&settings);
            saveAnimation(&settings);
            return true;
        }

        void deleteAnimation(int id)
        {
            if(id < 0 || id >= 100)return;
            
            String key = "a" + String(id);
            _storage.begin("anim_data", false);
            _storage.remove(key.c_str()); 
            _storage.end();
            
            if (animations[id] != nullptr) {
                delete animations[id];
                animations[id]=nullptr;
                animation_count--;
            }
        }

        int createAnimationsFromStorage()
        {
            String key = "";
            int found = 0;
            for (int i = 0; i < 100; i++)
            {
                key = "a" + String(i);
                AnimationSetting tempSettings;
                _storage.begin("anim_data", false);
                size_t len = _storage.getBytes(key.c_str(), &tempSettings, sizeof(AnimationSetting));
                if (len == sizeof(AnimationSetting)) 
                {   
                    createAnimation(&tempSettings, false);
                    found++;
                }
                _storage.end();
            }
            return found;
        }

        AnimationSetting* createSettingsStaticColor(unsigned long color, uint8_t brightness, String name)
        {
            if (name.length()>13)return nullptr;
            AnimationSetting* settings = new AnimationSetting();
            settings->type = STATIC_COLOR;
            memcpy(settings->name, name.c_str(), name.length());
            settings->data[0] = brightness;
            settings->data[1] = (uint8_t)(color & 0xFF);
            settings->data[2] = (uint8_t)((color >> 8) & 0xFF);
            settings->data[3] = (uint8_t)((color >> 16) & 0xFF);
            return settings;
        }

        AnimationSetting* createSettingsBlink(unsigned long color_on, unsigned long color_off, uint8_t cycle_ticks, uint8_t brightness, String name)
        {
            if (name.length()>13)return nullptr;
            AnimationSetting* settings = new AnimationSetting();
            settings->type = BLINK;
            memcpy(settings->name, name.c_str(), name.length());
            settings->data[0] = brightness;
            settings->data[1] = (uint8_t)(color_on & 0xFF);
            settings->data[2] = (uint8_t)((color_on >> 8) & 0xFF);
            settings->data[3] = (uint8_t)((color_on >> 16) & 0xFF);
            settings->data[4] = (uint8_t)(color_off & 0xFF);
            settings->data[5] = (uint8_t)((color_off >> 8) & 0xFF);
            settings->data[6] = (uint8_t)((color_off >> 16) & 0xFF);
            settings->data[7] = (uint8_t)cycle_ticks;
            return settings;
        }

        AnimationSetting* createSettingsPalette(uint8_t paletteID, uint8_t speed, uint8_t delta, uint8_t brightness, String name)
        {
            if (name.length() > 13) return nullptr;
            AnimationSetting* settings = new AnimationSetting();
            settings->type = PALETTE;
            memset(settings->name, 0, sizeof(settings->name));
            memcpy(settings->name, name.c_str(), name.length());
            settings->data[0] = brightness;
            settings->data[1] = paletteID;
            settings->data[2] = speed;
            settings->data[3] = delta;
            return settings;
        }

        AnimationSetting* createSettingsFire2D(uint8_t cooling_base, uint8_t sparking_chance, uint8_t paletteID, uint8_t brightness, String name)
        {
            if (name.length() > 13) return nullptr;
            AnimationSetting* settings = new AnimationSetting();
            settings->type = FIRE_2D;
            memset(settings->name, 0, sizeof(settings->name));
            memcpy(settings->name, name.c_str(), name.length());
            settings->data[0] = brightness;
            settings->data[1] = cooling_base;
            settings->data[2] = sparking_chance;
            settings->data[3] = paletteID;
            return settings;
        }

        // NEU: Factory-Methode für den Sonnenaufgang
        AnimationSetting* createSettingsSunrise(uint8_t duration_sec, uint8_t paletteID, uint8_t shimmer, uint8_t brightness, String name)
        {
            if (name.length() > 13) return nullptr;
            AnimationSetting* settings = new AnimationSetting();
            settings->type = SUNRISE;
            memset(settings->name, 0, sizeof(settings->name));
            memcpy(settings->name, name.c_str(), name.length());
            settings->data[0] = brightness;
            settings->data[1] = duration_sec;
            settings->data[2] = paletteID;
            settings->data[3] = shimmer;
            return settings;
        }

        int getAnimationCount()
        {
            return animation_count;
        }
    
    private:
        IAnimation* animations[100];
        CRGB *leds;
        int rgb_count = 0;
        Preferences _storage;
        int animation_count = 0;
};