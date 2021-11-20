#ifndef PLANT_HPP
#define PLANT_HPP

#include <Arduino.h>
#include <cstdint> 
#include <EEPROM.h>
#include <TimeLib.h>

#define MAX_PLANTS          8 // Maximum number of plants digital pins allow
#define EEPROM_STRUCT_WIDTH             (sizeof(struct EEPROMplant))
#define EEPROM_OFFSET_NAME_PLANT        (offsetof(struct EEPROMplant, name_plant))
#define EEPROM_OFFSET_IS_VALID          (offsetof(struct EEPROMplant, isValid))
#define EEPROM_OFFSET_FIRST_REGISTERED  (offsetof(struct EEPROMplant, firstRegistered))
#define EEPROM_OFFSET_LAST_WATERED      (offsetof(struct EEPROMplant, timeLastWatered))
#define EEPROM_OFFSET_INTERVAL_WATERING (offsetof(struct EEPROMplant, intervalWatering))

#define NUMBERFORMAT(arg)               (arg < 10 ? "0" + String(arg) : String(arg))

#define DEFAULT_INTERVAL_WATERING       (60 * 60 * 24) // 1 day

struct EEPROMplant
{
    char name_plant[128];
    uint8_t isValid;
    uint32_t firstRegistered;
    uint32_t timeLastWatered;
    uint32_t intervalWatering;
};




class Plant
{
private:
    uint32_t id;
    String namePlant;
    uint32_t firstRegistered;
    uint32_t timeLastWatered;
    uint32_t intervalWatering;

    static Plant* plants[MAX_PLANTS];
    
    Plant(String plantName, uint32_t currentTime, uint32_t intervalWatering);
    Plant(uint32_t id, String plantName, uint32_t firstRegistered, uint32_t timeLastWatered, uint32_t intervalWatering);
    ~Plant();

    static Plant* getPlantFromEEPROM(uint8_t index);
    static void writePlantToEEPROM(uint8_t index, Plant* plant);
    static bool EEPROMIsValid(int index);
public:
    static uint8_t numberOfPlants;

    String getName();
    String getLastWatered();
    String getNextWatering();
    String getIntervalWatering();
    String water();
    bool needsWatering();
    
    static void getPlantsFromEEPROM();


    static Plant* getPlant(uint8_t index);

    static void createPlant(String plantName, uint32_t currentTime, uint32_t intervalWatering);
    static void deletePlant(uint8_t index);

};
#endif // PLANT_HPP