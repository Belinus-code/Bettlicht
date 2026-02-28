#pragma once
#include <FastLED.h>
#define RGB_COUNT 89
#define STATIC_COLOR 1
#define BLINK 2
#define PALETTE 3

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
        virtual bool Update(unsigned long tick) = 0; //return true if LEDs needs to be flushed. 
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
            if(update_needed) //tick gets set to zero only if animation(program) got changed
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
        currentPalette = RainbowColors_p; // Standard
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
            case 2: currentPalette = OceanColors_p; break;     // Blau/Weiß/Türkis
            case 3: currentPalette = ForestColors_p; break;    // Grün/Braun
            case 4: currentPalette = HeatColors_p; break;      // Rot/Gelb/Weiß (Feuer)
            case 5: currentPalette = LavaColors_p; break;      // Rot/Schwarz/Orange
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
            case 0: // Palette ID
                if(value > 255) return false;
                ChangePalette((uint8_t)value);
                break;
            case 1: // Speed
                if(value > 255) return false;
                speed = (uint8_t)value;
                break;
            case 2: // Delta (Streckung)
                if(value > 255) return false;
                delta = (uint8_t)value;
                break;
            case 3: // Brightness
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
        return "0: Palette ID (0=Rainbow, 1=Party, 2=Ocean, 3=Forest, 4=Heat, 5=Lava, 6=Matrix)\n1: Speed\n2: Delta\n3: Brightness";
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