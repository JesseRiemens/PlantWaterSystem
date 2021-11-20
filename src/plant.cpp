#include <plant.hpp>

uint8_t Plant::numberOfPlants {0};
Plant* Plant::plants[MAX_PLANTS] {nullptr};

Plant::Plant(String plantName, uint32_t currentTime, uint32_t intervalWatering)
{
    if (plantName.length() > 128)
    {
        plantName = plantName.substring(0, 128);
    }
    this->namePlant = String(plantName);
    this->firstRegistered = currentTime;
    this->timeLastWatered = currentTime;
    this->intervalWatering = intervalWatering;
}

Plant::Plant(uint32_t id, String plantName, uint32_t firstRegistered, uint32_t timeLastWatered, uint32_t intervalWatering) {
    this->id = id;
    if (plantName.length() > 128)
    {
        plantName = plantName.substring(0, 128);
    }
    this->namePlant = String(plantName);
    this->firstRegistered = firstRegistered;
    this->timeLastWatered = timeLastWatered;
    this->intervalWatering = intervalWatering;
}

Plant::~Plant()
{
}

void Plant::getPlantsFromEEPROM() {
    Serial.println("Reading plants from EEPROM");
    for (int i = 0; i < MAX_PLANTS; i++)
    {
        if(Plant::EEPROMIsValid(i))
        {
            Serial.println("Reading plant " + String(i) + " from EEPROM");
            plants[i] = Plant::getPlantFromEEPROM(i);
            Serial.println("Plant " + String(i) + ": " + String(plants[i]->namePlant) + " " + plants[i]->firstRegistered + " " + plants[i]->timeLastWatered + " " + plants[i]->intervalWatering);
            Plant::numberOfPlants++;
        } else {
            continue;
        }
    }
}

Plant* Plant::getPlantFromEEPROM(uint8_t index) {
    uint32_t id = index;
    char namePlant[128];
    uint32_t firstRegistered, timeLastWatered, intervalWatering;
    EEPROM.get(index * EEPROM_STRUCT_WIDTH + 0, namePlant);
    EEPROM.get(index * EEPROM_STRUCT_WIDTH + EEPROM_OFFSET_FIRST_REGISTERED, firstRegistered);
    EEPROM.get(index * EEPROM_STRUCT_WIDTH + EEPROM_OFFSET_LAST_WATERED, timeLastWatered);
    EEPROM.get(index * EEPROM_STRUCT_WIDTH + EEPROM_OFFSET_INTERVAL_WATERING, intervalWatering);
    Serial.println("Plant " + String(index) + ": " + String(namePlant) + "; FirstRegistered " + firstRegistered + "; LastWatered " + timeLastWatered + "; IntervalWatering " + intervalWatering);
    return new Plant(id, String(namePlant), firstRegistered, timeLastWatered, intervalWatering);
}


void Plant::writePlantToEEPROM(uint8_t index, Plant* plant) {
    EEPROMplant eepromplant;
    plant->namePlant.toCharArray(eepromplant.name_plant, 128);
    eepromplant.isValid = true;
    eepromplant.firstRegistered = plant->firstRegistered;
    eepromplant.timeLastWatered = plant->timeLastWatered;
    eepromplant.intervalWatering = plant->intervalWatering;
    EEPROM.put(index * EEPROM_STRUCT_WIDTH, eepromplant);
    EEPROM.commit();
}

bool Plant::EEPROMIsValid(int index) {
    return EEPROM.read(index * EEPROM_STRUCT_WIDTH + EEPROM_OFFSET_IS_VALID) == true;
}

Plant* Plant::getPlant(uint8_t index) {
    return plants[index];
}

String Plant::getName() {
    return this->namePlant;
}

String Plant::getLastWatered() {
    time_t timeLastWatered = this->timeLastWatered;
    return String(day(timeLastWatered)) + " " + String(monthStr(month(timeLastWatered))) + " " + year(timeLastWatered) + " " + NUMBERFORMAT(hour(timeLastWatered)) + ":" + NUMBERFORMAT(minute(timeLastWatered));
}

String Plant::getNextWatering() {
    time_t timeLastWatered = this->timeLastWatered;
    time_t nextWatering = timeLastWatered + this->intervalWatering;
    return String(day(nextWatering)) + " " + String(monthStr(month(nextWatering))) + " " + year(nextWatering) + " " + NUMBERFORMAT(hour(nextWatering)) + ":" + NUMBERFORMAT(minute(nextWatering));
}

String Plant::getIntervalWatering() {
    String ret = "";
    time_t intervalWatering = this->intervalWatering;
    ret += month(intervalWatering)  > 1 ? String(month(intervalWatering) - 1) + " maanden" : "";
    ret += day(intervalWatering)    > 1 ? (ret != ""? ", " : "") + String(day(intervalWatering) - 1) + " dagen" : "";
    ret += hour(intervalWatering)   > 1 ? (ret != ""? ", " : "") + String(hour(intervalWatering)) + " uur" : "";
    ret += minute(intervalWatering) > 1 ? (ret != ""? ", " : "") + String(minute(intervalWatering)) + " minuten" : "";
    return ret;
}

// See if intervalWatering + timeLastWatered is in the past. If so, return true.
bool Plant::needsWatering() {
    time_t timeLastWatered = this->timeLastWatered;
    time_t nextWatering = timeLastWatered + this->intervalWatering;
    return nextWatering < now();
}

String Plant::water() {
    time_t currentTime = now();
    this->timeLastWatered = currentTime;
    writePlantToEEPROM(this->id, this);
    EEPROM.commit();
    return "Plant " + this->namePlant + " is gewaterd";
}

void Plant::createPlant(String plantName, uint32_t currentTime, uint32_t intervalWatering) {
    if (numberOfPlants < MAX_PLANTS) {
        for (int i = 0; i < MAX_PLANTS; i++)
        {
            if (plants[i] == nullptr)
            {
                if(intervalWatering == 0)
                {
                    intervalWatering = DEFAULT_INTERVAL_WATERING;
                }
                plants[i] = new Plant(plantName, currentTime, intervalWatering);
                numberOfPlants++;
                // Serial.println("Plant " + String(i) + ": " + String(plants[i]->namePlant) + " " + plants[i]->firstRegistered + " " + plants[i]->timeLastWatered + " " + plants[i]->intervalWatering);
                writePlantToEEPROM(i, plants[i]);
                return;
            }
        }
    }
}

void Plant::deletePlant(uint8_t index) {
    if (numberOfPlants > 0)
    {
        delete plants[index];
        plants[index] = nullptr;
        numberOfPlants--;
        // clear plant from EEPROM
        EEPROM.put(index * EEPROM_STRUCT_WIDTH, EEPROMplant());
        EEPROM.commit();
        
    } 
 }