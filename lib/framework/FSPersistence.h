#ifndef FSPersistence_h
#define FSPersistence_h

#include <StatefulService.h>
#include <FS.h>

template <class T>
class FSPersistence {
  public:
    FSPersistence(JsonStateReader<T>   stateReader,
                  JsonStateUpdater<T>  stateUpdater,
                  StatefulService<T> * statefulService,
                  FS *                 fs,
                  const char *         filePath,
                  size_t               bufferSize = FS_BUFFER_SIZE)
        : _stateReader(stateReader)
        , _stateUpdater(stateUpdater)
        , _statefulService(statefulService)
        , _fs(fs)
        , _filePath(filePath)
        , _bufferSize(bufferSize)
        , _updateHandlerId(0) {
        enableUpdateHandler();
    }

    void readFromFS() {
        File settingsFile = _fs->open(_filePath, "r");

        if (settingsFile) {
            DynamicJsonDocument  jsonDocument = DynamicJsonDocument(_bufferSize);
            DeserializationError error        = deserializeJson(jsonDocument, settingsFile);
            if (error == DeserializationError::Ok && jsonDocument.is<JsonObject>()) {
                // jsonDocument.shrinkToFit(); // added by proddy
                JsonObject jsonObject = jsonDocument.as<JsonObject>();

// debug added by Proddy
#if defined(EMSESP_DEBUG)
                Serial.printf("Reading file: %s: ", _filePath);
                serializeJson(jsonDocument, Serial);
                Serial.println();
#endif

                _statefulService->updateWithoutPropagation(jsonObject, _stateUpdater);
                settingsFile.close();
                return;
            }
            settingsFile.close();
        }

        // If we reach here we have not been successful in loading the config,
        // hard-coded emergency defaults are now applied.
        applyDefaults();
    }

    bool writeToFS() {
        // create and populate a new json object
        DynamicJsonDocument jsonDocument = DynamicJsonDocument(_bufferSize);
        JsonObject          jsonObject   = jsonDocument.to<JsonObject>();
        _statefulService->read(jsonObject, _stateReader);

        // serialize it to filesystem
        File settingsFile = _fs->open(_filePath, "w");

        // failed to open file, return false
        if (!settingsFile) {
            return false;
        }

// debug added by Proddy
#if defined(EMSESP_DEBUG)
        Serial.printf("Writing to file: %s: ", _filePath);
        serializeJson(jsonDocument, Serial);
        Serial.println();
#endif

        // serialize the data to the file
        serializeJson(jsonDocument, settingsFile);
        settingsFile.close();
        return true;
    }

    void disableUpdateHandler() {
        if (_updateHandlerId) {
            _statefulService->removeUpdateHandler(_updateHandlerId);
            _updateHandlerId = 0;
        }
    }

    void enableUpdateHandler() {
        if (!_updateHandlerId) {
            _updateHandlerId = _statefulService->addUpdateHandler([&](const String & originId) { writeToFS(); });
        }
    }

  private:
    JsonStateReader<T>   _stateReader;
    JsonStateUpdater<T>  _stateUpdater;
    StatefulService<T> * _statefulService;
    FS *                 _fs;
    const char *         _filePath;
    size_t               _bufferSize;
    update_handler_id_t  _updateHandlerId;

  protected:
    // We assume the updater supplies sensible defaults if an empty object
    // is supplied, this virtual function allows that to be changed.
    virtual void applyDefaults() {
        DynamicJsonDocument jsonDocument = DynamicJsonDocument(_bufferSize);
        JsonObject          jsonObject   = jsonDocument.as<JsonObject>();
        _statefulService->updateWithoutPropagation(jsonObject, _stateUpdater);
    }
};

#endif // end FSPersistence
