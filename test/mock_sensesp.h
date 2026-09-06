// Mock SensESP classes for native unit testing
// Provides minimal implementations needed to test service logic

#ifndef MOCK_SENSESP_H
#define MOCK_SENSESP_H

#include <stdint.h>
#include <vector>
#include <functional>
#include <memory>
#include <string>

namespace sensesp {

// Mock metadata
class SKMetadata {
public:
    SKMetadata(const char* units = nullptr) : units_(units) {}
    const char* units_;
};

// Mock listener base class
class SKListener {
public:
    virtual ~SKListener() = default;
};

// Mock bool listener
class BoolSKListener : public SKListener {
public:
    BoolSKListener(const char* sk_path) : sk_path_(sk_path), listener_(nullptr) {}
    const char* sk_path_;
    std::function<void(bool)>* listener_;
    
    template <typename T>
    void connect_to(T* consumer) {
        listener_ = new std::function<void(bool)>([consumer](bool val) {
            consumer->set_input(val);
        });
    }
    
    void trigger(bool value) {
        if (listener_) (*listener_)(value);
    }
};

// Mock int listener
class IntSKListener : public SKListener {
public:
    IntSKListener(const char* sk_path) : sk_path_(sk_path), listener_(nullptr) {}
    const char* sk_path_;
    std::function<void(int)>* listener_;
    
    template <typename T>
    void connect_to(T* consumer) {
        listener_ = new std::function<void(int)>([consumer](int val) {
            consumer->set_input(val);
        });
    }
    
    void trigger(int value) {
        if (listener_) (*listener_)(value);
    }
};

// Mock float listener
class FloatSKListener : public SKListener {
public:
    FloatSKListener(const char* sk_path) : sk_path_(sk_path), listener_(nullptr) {}
    const char* sk_path_;
    std::function<void(float)>* listener_;
    
    template <typename T>
    void connect_to(T* consumer) {
        listener_ = new std::function<void(float)>([consumer](float val) {
            consumer->set_input(val);
        });
    }
    
    void trigger(float value) {
        if (listener_) (*listener_)(value);
    }
};

// Mock string listener
class StringSKListener : public SKListener {
public:
    StringSKListener(const char* sk_path) : sk_path_(sk_path), listener_(nullptr) {}
    const char* sk_path_;
    std::function<void(std::string)>* listener_;

    template <typename T>
    void connect_to(T* consumer) {
        listener_ = new std::function<void(std::string)>([consumer](const std::string& val) {
            consumer->set_input(val);
        });
    }

    void trigger(const std::string& value) {
        if (listener_) (*listener_)(value);
    }
};

// Mock observable value
template <typename T>
class ObservableValue {
public:
    ObservableValue() : value_(T()), output_(nullptr) {}
    virtual ~ObservableValue() = default;
    
    void set(T value) { value_ = value; }
    T get() const { return value_; }
    
    void notify() {
        // In real implementation, this would notify consumers
    }
    
    void connect_to(void* output) {
        output_ = output;
    }
    
    T value_;
    void* output_;
};

// Mock SKOutput base class
class SKOutput {
public:
    SKOutput(const char* sk_path, const char* config_path) 
        : sk_path_(sk_path), config_path_(config_path), metadata_(nullptr) {}
    virtual ~SKOutput() = default;
    
    void set_metadata(SKMetadata* metadata) { metadata_ = metadata; }
    const char* sk_path_;
    const char* config_path_;
    SKMetadata* metadata_;
};

// Mock SKOutputBool
class SKOutputBool : public SKOutput {
public:
    SKOutputBool(const char* sk_path, const char* config_path) 
        : SKOutput(sk_path, config_path), value_(false) {}
    
    void set_input(bool value) { value_ = value; }
    bool get_input() const { return value_; }
    
    bool value_;
};

// Mock SKOutputInt
class SKOutputInt : public SKOutput {
public:
    SKOutputInt(const char* sk_path, const char* config_path) 
        : SKOutput(sk_path, config_path), value_(0) {}
    
    void set_input(int value) { value_ = value; }
    int get_input() const { return value_; }
    
    int value_;
};

// Mock SKOutputFloat
class SKOutputFloat : public SKOutput {
public:
    SKOutputFloat(const char* sk_path, const char* config_path) 
        : SKOutput(sk_path, config_path), value_(0.0f) {}
    
    void set_input(float value) { value_ = value; }
    float get_input() const { return value_; }
    
    float value_;
};

// Mock SKOutputString
class SKOutputString : public SKOutput {
public:
    SKOutputString(const char* sk_path, const char* config_path)
        : SKOutput(sk_path, config_path), value_() {}

    void set_input(const std::string& value) { value_ = value; }
    const std::string& get_input() const { return value_; }

    std::string value_;
};

// Mock lambda transform
template <typename Input, typename Output>
class LambdaTransform {
public:
    LambdaTransform(std::function<Output(Input)> fn) : fn_(fn) {}
    
    Output process(Input value) {
        return fn_(value);
    }
    
    LambdaTransform<Output, Output>* connect_to(SKOutput* output) {
        output_ = output;
        return new LambdaTransform<Output, Output>([this](Output val) {
            return val;
        });
    }
    
    std::function<Output(Input)> fn_;
    SKOutput* output_;
};

} // namespace sensesp

#endif // MOCK_SENSESP_H
