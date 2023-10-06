#include "EngineProxy.h"
#include "PluginConsole.h"
#include "audio_bus.h"
#include "audio_parameter.h"
#include "audio_effect.h"
#include <cassert>

template<class C, class WrapperClass>
class Wrapper : public script::ScriptClass
{
public:

    Wrapper(const script::Local<script::Object>& self)
        : script::ScriptClass(self)
        , wrappedObject{ nullptr }
    {
    }

    void setObject(C* obj)
    {
        wrappedObject = obj;
    }

    template<typename T>
    void addProperty(script::Local<script::Object>& obj,
                const std::string& name,
                std::function<T()> getter,
                std::function<void(T)> setter = {})
    {
        auto* scriptEngine{ getScriptEngine() };

        script::EngineScope scope(scriptEngine);

        auto Object{ scriptEngine->get("Object").asObject() };
        auto defineProperty{ Object.get("defineProperty").asFunction() };

        auto getterSetterPair{ script::Object::newObject() };

        if (getter)
            getterSetterPair.set("get", script::Function::newFunction(std::move(getter)));

        if (setter)
            getterSetterPair.set("set", script::Function::newFunction(std::move(setter)));

        defineProperty.call(Object, obj, name, getterSetterPair);
    }

    virtual void exposeCustomProperties(script::Local<script::Object>&)
    {
        auto* scriptEngine{ getScriptEngine() };
        assert(scriptEngine != nullptr);
    }

    static script::Local<script::Value> createInstance(script::ScriptEngine* scriptEngine, C* obj)
    {
        assert(scriptEngine != nullptr);

        script::Local<script::Object> wrapperObj{ scriptEngine->newNativeClass<WrapperClass>() };
        auto* wrapper{ scriptEngine->getNativeInstance<WrapperClass>(wrapperObj) };
        wrapper->setObject(obj);
        wrapper->exposeCustomProperties(wrapperObj);

        return wrapperObj;
    }

    C* getWrappedObject() noexcept { return wrappedObject; }

protected:
    C* wrappedObject{ nullptr };
};

//==============================================================================

class ConsoleWrapper : public Wrapper<Console, ConsoleWrapper>
{
public:

    ConsoleWrapper(const script::Local<script::Object>& self)
        : Wrapper<Console, ConsoleWrapper>(self)
    {}

    script::Local<script::Value> log(const script::Arguments& args)
    {
        if (wrappedObject == nullptr) {
            jassertfalse;
            return {};
        }

        auto* scriptEngine{ args.engine() };

        String msg;

        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i].isString()) {
                msg += String(args[i].asString().toString());
            } else {
                msg += String(args[i].describeUtf8());
            }

            if (i + 1 < args.size())
                msg += " ";
        }

        wrappedObject->postMessage(msg);

        return {};
    }

    static void registerWithScriptEngine(script::ScriptEngine* scriptEngine)
    {
        static const auto wrapperClassDef{
            script::defineClass<ConsoleWrapper>("Console")
                .constructor()
                .instanceFunction("log", &ConsoleWrapper::log)
                .build()
        };

        scriptEngine->registerNativeClass(wrapperClassDef);
    }

};

//==============================================================================

class AudioParameterWrapper : public Wrapper<tonewheel::AudioParameter, AudioParameterWrapper>
{
public:
    AudioParameterWrapper(const script::Local<script::Object>& self)
        : Wrapper<tonewheel::AudioParameter, AudioParameterWrapper>(self)
    {}

    float getValue() const
    {
        assert(wrappedObject != nullptr);
        return wrappedObject->getTargetValue();
    }

    void setValue(float x)
    {
        assert(wrappedObject != nullptr);
        wrappedObject->setValue(x);
    }

    static void registerWithScriptEngine(script::ScriptEngine* scriptEngine)
    {
        static const auto wrapperClassDef{
            script::defineClass<AudioParameterWrapper>("AudioParameter")
                .constructor()
                .instanceProperty("value", &AudioParameterWrapper::getValue, &AudioParameterWrapper::setValue)
                .build()
        };

        scriptEngine->registerNativeClass(wrapperClassDef);
    }
};

//==============================================================================

class AudioEffectWrapper : public Wrapper<tonewheel::AudioEffect, AudioEffectWrapper>
{
public:
    AudioEffectWrapper(const script::Local<script::Object>& self)
        : Wrapper<tonewheel::AudioEffect, AudioEffectWrapper>(self)
    {}

    void exposeCustomProperties(script::Local<script::Object>& obj) override
    {
        assert(wrappedObject != nullptr);

        auto* scriptEngine{ getScriptEngine() };
        assert(scriptEngine != nullptr);

        auto& params{ wrappedObject->getParameters() };

        for (int i = 0; i < params.getNumParameters(); ++i) {
            auto& param{ params[i] };

            addProperty<float>(obj, param.getName(),
                [&param]() { return param.getTargetValue(); },
                [&param](float x) { param.setValue(x); }
            );
        }
    }

    std::string getTag() const
    {
        assert(wrappedObject != nullptr);
        return wrappedObject->getTag();
    }

    script::Local<script::Value> getParameters()
    {
        assert(wrappedObject != nullptr);

        auto* scriptEngine{ getScriptEngine() };
        auto& paramsPool{ wrappedObject->getParameters() };

        auto paramsObj{ script::Object::newObject() };

        for (int i = 0; i < paramsPool.getNumParameters(); ++i) {
            auto& param{ paramsPool[i] };

            auto wrapperObj{ AudioParameterWrapper::createInstance(scriptEngine, &param) };
            paramsObj.set(param.getName(), wrapperObj);
        }

        return paramsObj;
    }

    void setParameterValue(const std::string& name, float value)
    {
        assert(wrappedObject != nullptr);
        auto& param{ wrappedObject->getParameters().getParameterByName(name) };
        param.setValue(value);
    }

    float getParameterValue(const std::string& name) const
    {
        assert(wrappedObject != nullptr);
        auto& param{ wrappedObject->getParameters().getParameterByName(name) };
        return param.getTargetValue();
    }

    static void registerWithScriptEngine(script::ScriptEngine* scriptEngine)
    {
        static const auto wrapperClassDef{
            script::defineClass<AudioEffectWrapper>("AudioEffect")
                .constructor()
                .instanceProperty("parameters", &AudioEffectWrapper::getParameters)
                .instanceFunction("setParameter", &AudioEffectWrapper::setParameterValue)
                .instanceFunction("getParameter", &AudioEffectWrapper::getParameterValue)
                .instanceProperty("tag", &AudioEffectWrapper::getTag)
                .build()
        };

        scriptEngine->registerNativeClass(wrapperClassDef);
    }
};

//==============================================================================

class AudioBusWrapper : public Wrapper<tonewheel::AudioBus, AudioBusWrapper>
{
public:
    AudioBusWrapper(const script::Local<script::Object>& self)
        : Wrapper<tonewheel::AudioBus, AudioBusWrapper>(self)
    {}

    script::Local<script::Value> wrapEffect(tonewheel::AudioEffect* fx)
    {
        auto* scriptEngine{ getScriptEngine() };
        return AudioEffectWrapper::createInstance(scriptEngine, fx);
    }

    script::Local<script::Value> addEffect(const std::string& tag)
    {
        assert(wrappedObject != nullptr);

        auto& fxChain{ wrappedObject->getFxChain() };

        if (auto* fx{ fxChain.addEffectByTag(tag) })
            return AudioEffectWrapper::createInstance(getScriptEngine(), fx);

        return {};
    }

    script::Local<script::Value> getEffects()
    {
        assert(wrappedObject != nullptr);

        auto* scriptEngine{ getScriptEngine() };
        auto& fxChain{ wrappedObject->getFxChain() };

        std::vector<script::Local<script::Value>> effects;

        for (int i = 0; i < fxChain.getNumEffects(); ++i) {
            auto* fx{ fxChain[i] };
            effects.push_back(AudioEffectWrapper::createInstance(scriptEngine, fx));
        }

        return script::Array::newArray(effects);
    }

    float getGain() const
    {
        assert(wrappedObject != nullptr);
        return wrappedObject->getParameters()[tonewheel::AudioBus::GAIN].getTargetValue();
    }

    void setGain(float gain)
    {
        assert(wrappedObject != nullptr);
        wrappedObject->getParameters()[tonewheel::AudioBus::GAIN].setValue(gain);
    }

    float getPan() const
    {
        assert(wrappedObject != nullptr);
        return wrappedObject->getParameters()[tonewheel::AudioBus::PAN].getTargetValue();
    }

    void setPan(float pan)
    {
        assert(wrappedObject != nullptr);
        wrappedObject->getParameters()[tonewheel::AudioBus::PAN].setValue(pan);
    }

    static void registerWithScriptEngine(script::ScriptEngine* scriptEngine)
    {
        static const auto wrapperClassDef{
            script::defineClass<AudioBusWrapper>("AudioBus")
                .constructor()
                .instanceFunction("addEffect", &AudioBusWrapper::addEffect)
                .instanceProperty("effects",   &AudioBusWrapper::getEffects)
                .instanceProperty("gain",      &AudioBusWrapper::getGain, &AudioBusWrapper::setGain)
                .instanceProperty("pan",       &AudioBusWrapper::getPan,  &AudioBusWrapper::setPan)
                .build()
        };

        scriptEngine->registerNativeClass(wrapperClassDef);
    }
};

//==============================================================================

class EngineWrapper : public Wrapper<tonewheel::Engine, EngineWrapper>
{
public:
    EngineWrapper(const script::Local<script::Object>& self)
        : Wrapper<tonewheel::Engine, EngineWrapper>(self)
    {}

    int addSample(const std::string& filePath)
    {
        assert(wrappedObject != nullptr);
        return wrappedObject->addSample(filePath);
    }

    int addSampleWithRange(const std::string& filePath, int startPos, int stopPos)
    {
        assert(wrappedObject != nullptr);
        return wrappedObject->addSample(filePath, startPos, stopPos);
    }

    double getBpm() const
    {
        assert(wrappedObject != nullptr);
        return wrappedObject->getTransportInfo().bpm;
    }

    double getTime() const
    {
        assert(wrappedObject != nullptr);
        return wrappedObject->getTransportInfo().time;
    }

    double getMusicTime() const
    {
        assert(wrappedObject != nullptr);
        return wrappedObject->getTransportInfo().ppqPosition;
    }

    script::Local<script::Value> getBuses()
    {
        assert(wrappedObject != nullptr);

        auto* scriptEngine{ getScriptEngine() };
        auto& audioBusPool{ wrappedObject->getAudioBusPool() };

        std::vector<script::Local<script::Value>> buses;

        for (int i = 0; i < audioBusPool.getNumBuses(); ++i)
            buses.push_back(getBus(i));

        return script::Array::newArray(buses);
    }

    script::Local<script::Value> getBus(int index)
    {
        assert(wrappedObject != nullptr);
        auto& audioBusPool{ wrappedObject->getAudioBusPool() };

        if (index < 0 || index >= audioBusPool.getNumBuses())
            return {};

        auto* scriptEngine{ getScriptEngine() };
        return AudioBusWrapper::createInstance(scriptEngine, &audioBusPool[index]);
    }

    void addVoiceTriggerEffect(tonewheel::Engine::Trigger& trigger, const script::Local<script::Object>& desc)
    {
        assert(wrappedObject != nullptr);

        if (trigger.fxChain == nullptr) {
            trigger.fxChain.reset(new tonewheel::AudioEffectChain);
            trigger.fxChain->setEngine(wrappedObject);
        }

        if (desc.has("tag")) {
            std::string tag{ desc.get("tag").asString().toString() };

            if (auto* fx{ trigger.fxChain->addEffectByTag(tag) }) {
                auto& params{ fx->getParameters() };

                for (const auto& key : desc.getKeys()) {
                    std::string name{ key.toString() };

                    if (name == "id") {
                        fx->setId(desc.get(key).asString().toString());
                    } else if (name != "tag") {
                        auto& param{ params.getParameterByName(name) };
                        param.setValue(desc.get(key).asNumber().toFloat(), true);
                    }
                }
            }
        }
    }

    void addVoiceTriggerModulation(tonewheel::Engine::Trigger& trigger, const script::Local<script::Object>& desc)
    {
        assert(wrappedObject != nullptr);

        std::string expr{ desc.get("expr").asString().toString() };

        if (expr.empty())
            return;

        auto modulator{ std::make_shared<tonewheel::Voice::Modulator>() };

        modulator->addConstant("sampleRate", wrappedObject->getSampleRate());
        modulator->addConstant("bpm", wrappedObject->getTransportInfo().bpm);
        modulator->addConstant("ppq", wrappedObject->getTransportInfo().ppqPosition);

        std::map<std::string, float> dynVars;

        for (const auto& key : desc.getKeys()) {
            std::string name{ key.toString() };

            if (name != "expr")
                dynVars[name] = desc.get(key).asNumber().toFloat();
        }

        modulator->addDynamicVariables(dynVars);

        // FX-chain parameters
        if (trigger.fxChain != nullptr) {
            for (int i = 0; i < trigger.fxChain->getNumEffects(); ++i) {
                auto* effect{ trigger.fxChain->getEffectByIndex(i) };
                const auto fxId{ effect->getId() };

                if (!fxId.empty()) {
                    auto& params{ effect->getParameters() };

                    for (int j = 0; j < params.getNumParameters(); ++j) {
                        auto& param{ params[j] };
                        const auto& name{ param.getName() };

                        if (!name.empty()) {
                            std::string sfxName{ fxId + "." + name };
                            modulator->addVariable(name, param.getTargetRef());
                        }
                    }
                }
            }
        }

        // Expose cc[] array
        modulator->addVector("cc", wrappedObject->getCCParameters());

        if (modulator->compile(expr))
            trigger.modulator = std::move(modulator);
        else
            DBG("*** Unable to compile modulator " << expr);
    }

    script::Local<script::Value> trigger(const script::Arguments& args)
    {
        assert(wrappedObject != nullptr);

        if (args.size() != 1)
            return {};

        if (!args[0].isObject())
            return {};

        auto arg{ args[0].asObject() };

        tonewheel::Engine::Trigger trigger{};

        if (arg.has("sample"))
            trigger.sampleId = arg.get("sample").asNumber().toInt32();
        if (arg.has("bus"))
            trigger.busNumber = arg.get("bus").asNumber().toInt32();
        if (arg.has("key"))
            trigger.key = arg.get("key").asNumber().toInt32();
        if (arg.has("rootKey"))
            trigger.rootKey = arg.get("rootKey").asNumber().toInt32();
        if (arg.has("offset"))
            trigger.offset = arg.get("offset").asNumber().toInt32();
        if (arg.has("gain"))
            trigger.gain = arg.get("gain").asNumber().toFloat();
        if (arg.has("tune"))
            trigger.tune = arg.get("tune").asNumber().toFloat();
        if (arg.has("loop") && arg.get("loop").isObject()) {
            auto loopObj{ arg.get("loop").asObject() };
            trigger.loopBegin = loopObj.has("begin") ? loopObj.get("begin").asNumber().toInt32() : 0;
            trigger.loopEnd = loopObj.has("end") ? loopObj.get("end").asNumber().toInt32() : 0;
            if (loopObj.has("xfade"))
                trigger.loopXfade = loopObj.get("xfade").asNumber().toInt32();
        }
        if (arg.has("envelope") && arg.get("envelope").isObject()) {
            auto envObj{ arg.get("envelope").asObject() };
            if (envObj.has("attack"))
                trigger.envelope.attack = envObj.get("attack").asNumber().toFloat();
            if (envObj.has("decay"))
                trigger.envelope.decay = envObj.get("decay").asNumber().toFloat();
            if (envObj.has("sustain"))
                trigger.envelope.sustain = envObj.get("sustain").asNumber().toFloat();
            if (envObj.has("release"))
                trigger.envelope.release = envObj.get("release").asNumber().toFloat();
        }
        if (arg.has("fx") && arg.get("fx").isArray()) {
            auto fxArray{ arg.get("fx").asArray() };
            for (size_t i = 0; i < fxArray.size(); ++i) {
                auto item{ fxArray.get(i) };
                if (item.isObject()) {
                    auto obj{ item.asObject() };
                    addVoiceTriggerEffect(trigger, obj);
                }
            }
        }
        if (arg.has("modulate") && arg.get("modulate").isObject())
        {
            auto obj{ arg.get("modulate").asObject() };
            addVoiceTriggerModulation(trigger, obj);
        }

        auto voiceId{ wrappedObject->triggerVoice(trigger) };

        return script::Number::newNumber(voiceId);
    }

    void release(int voiceId)
    {
        assert(wrappedObject != nullptr);
        wrappedObject->releaseVoice(voiceId);
    }

    void releaseWithTime(int voiceId, float t)
    {
        assert(wrappedObject != nullptr);
        wrappedObject->releaseVoice(voiceId, t);
    }

    float getCC(int index)
    {
        return wrappedObject->getCC(index);
    }

    void setCC(int index, float value)
    {
        wrappedObject->setCC(index, value);
    }

    static void registerWithScriptEngine(script::ScriptEngine* scriptEngine)
    {
        const static auto wrapperClassDef{
            script::defineClass<EngineWrapper>("Engine")
                .constructor()
                .instanceFunction("addSample",          &EngineWrapper::addSample)
                .instanceFunction("addSampleWithRange", &EngineWrapper::addSampleWithRange)
                .instanceProperty("bpm",                &EngineWrapper::getBpm)
                .instanceProperty("time",               &EngineWrapper::getTime)
                .instanceProperty("musicTime",          &EngineWrapper::getMusicTime)
                .instanceFunction("getBus",             &EngineWrapper::getBus)
                .instanceProperty("bus",                &EngineWrapper::getBuses)
                .instanceFunction("trigger",            &EngineWrapper::trigger)
                .instanceFunction("release",            &EngineWrapper::release)
                .instanceFunction("releaseWithTime",    &EngineWrapper::releaseWithTime)
                .instanceFunction("getCC",              &EngineWrapper::getCC)
                .instanceFunction("setCC",              &EngineWrapper::setCC)
                .build()
        };

        scriptEngine->registerNativeClass(wrapperClassDef);
    }
};

//==============================================================================

script::Local<script::Value> setTimeout(const script::Arguments& args)
{
    if (args.size() == 0)
        return {};

    auto callback{ script::Global<script::Function>(args[0].asFunction()) };
    int64_t delayMs{ 0 };

    if (args.size() > 1)
        delayMs =args[1].asNumber().toInt64();

    auto* engine{ args.engine() };

    auto runner = [fun = std::move(callback), engine]() {
        script::EngineScope scope(engine);

        try {
            fun.get().call();
        } catch (const script::Exception& e) {
            DBG(e.what());
            DBG(e.stacktrace());
        }
    };

    using RunnerType = decltype(runner);

    auto&& queue{ engine->messageQueue() };
    auto msg = queue->obtainInplaceMessage([](script::utils::InplaceMessage& msg) {
        msg.getObject<RunnerType>()();
    });
    msg->inplaceObject<RunnerType>(std::move(runner));
    queue->postMessage(msg, std::chrono::milliseconds(delayMs));

    return {};
}

//==============================================================================

EngineProxy::EngineProxy(tonewheel::Engine& eng, Console& con)
    : juce::Thread("EngineProxy")
    , engine{ eng }
    , console{ con }
    , scriptEngine{}
{
}

EngineProxy::~EngineProxy()
{
    stop();
}

void EngineProxy::start(const String& code)
{
    scriptEngine.reset(new script::ScriptEngineImpl(), script::ScriptEngine::Deleter());

    registerGlobals();

    // Perform script evaluation in order to initialize the context
    eval(code);

    startThread();
}

void EngineProxy::stop()
{
    if (scriptEngine == nullptr)
        return;

    signalThreadShouldExit();
    scriptEngine->messageQueue()->shutdownNow(true);
    waitForThreadToExit(-1);
    scriptEngine.reset();
}

void EngineProxy::setContentFolder(const File& dir)
{
    contentFolder = dir;
}

void EngineProxy::postMessage(std::function<void()> func)
{
    auto* engine{ scriptEngine.get() };
    script::EngineScope scope(engine);
    auto callback{ script::Global<script::Function>(script::Function::newFunction(std::move(func))) };

    auto runner = [fun = std::move(callback), engine]() {
        script::EngineScope scope(engine);

        try {
            fun.get().call();
        } catch (const script::Exception& e) {
            DBG(e.what());
            DBG(e.stacktrace());
        }
    };

    using RunnerType = decltype(runner);

    auto&& queue{ engine->messageQueue() };
    auto msg = queue->obtainInplaceMessage([](script::utils::InplaceMessage& msg) {
        msg.getObject<RunnerType>()();
    });
    msg->inplaceObject<RunnerType>(std::move(runner));
    auto id{ queue->postMessage(msg) };
    assert(id != 0);
}

void EngineProxy::postMidiMessage(const MidiMessage& midiMessage)
{
    // @todo This will allocate memory.
    //       Instead we need to queue midiMessages into the lock-free
    //       queue and then process them through the event loop.
    postMessage([this, msg=midiMessage] {
        callOnMidiMessage(msg);
    });
}

void EngineProxy::postMidiMessage(const tonewheel::MidiMessage& midiMessage, bool notify)
{
    midiBuffer.send(midiMessage);

    if (notify)
      scriptEngine->messageQueue()->interrupt();
}

void EngineProxy::notify()
{
    scriptEngine->messageQueue()->interrupt();
}

bool EngineProxy::sendMidiMessage(const MidiMessage& midiMessage)
{
    auto wait{ std::make_shared<juce::WaitableEvent>() };

    postMessage([this, wait, msg = midiMessage] {
        callOnMidiMessage(msg);
        wait->signal();
    });

    return wait->wait(100);
}

bool EngineProxy::sendMidiMessage(const tonewheel::MidiMessage& midiMessage)
{
    auto wait{ std::make_shared<juce::WaitableEvent>() };

    postMessage([this, wait, msg = midiMessage] {
        callOnMidiMessage(msg);
        wait->signal();
    });

    return wait->wait(100);
}

void EngineProxy::run()
{
    while (!threadShouldExit()) {
        // @todo handle message loop interruption here
        tonewheel::MidiMessage msg;

        while (midiBuffer.receive(msg)) {
            callOnMidiMessage(msg);
        }

        scriptEngine->messageQueue()->loopQueue(script::utils::MessageQueue::LoopType::kLoopAndWait);
    }
}

void EngineProxy::registerGlobals()
{
    script::EngineScope scope(scriptEngine.get());

    scriptEngine->set(script::String::newString(u8"setTimeout"), script::Function::newFunction(setTimeout));

    ConsoleWrapper::registerWithScriptEngine(scriptEngine.get());
    AudioParameterWrapper::registerWithScriptEngine(scriptEngine.get());
    AudioEffectWrapper::registerWithScriptEngine(scriptEngine.get());
    AudioBusWrapper::registerWithScriptEngine(scriptEngine.get());
    EngineWrapper::registerWithScriptEngine(scriptEngine.get());

    scriptEngine->set(script::String::newString(u8"engine"), EngineWrapper::createInstance(scriptEngine.get(), &engine));
    scriptEngine->set(script::String::newString(u8"console"), ConsoleWrapper::createInstance(scriptEngine.get(), &console));

    scriptEngine->set(script::String::newString(u8"$dir"), script::String::newString(contentFolder.getFullPathName().toStdString()));

    scriptEngine->messageQueue()->loopQueue(script::utils::MessageQueue::LoopType::kLoopOnce);
}

void EngineProxy::eval(const String& code)
{
    script::EngineScope scope(scriptEngine.get());

    try {
        scriptEngine->eval(code.toStdString());
    } catch (const script::Exception& e) {
        console.postMessage("*** Error ***");
        console.postMessage(e.what());
        console.postMessage(e.stacktrace());
    }
}

void EngineProxy::callOnMidiMessage(const MidiMessage& msg)
{
    script::EngineScope scope(scriptEngine.get());

    script::Local<script::Value> f{ scriptEngine->get("onMidiMessage") };

    if (f.isFunction()) {
        auto onMidiMessage{ f.asFunction() };

        auto obj{ script::Object::newObject() };
        obj.set(script::String::newString(u8"noteOn"),           script::Boolean::newBoolean(msg.isNoteOn()));
        obj.set(script::String::newString(u8"noteOff"),          script::Boolean::newBoolean(msg.isNoteOff()));
        obj.set(script::String::newString(u8"controller"),       script::Boolean::newBoolean(msg.isController()));

        if (msg.isNoteOn() || msg.isNoteOff()) {
            obj.set(script::String::newString(u8"noteNumber"),       script::Number::newNumber(msg.getNoteNumber()));
            obj.set(script::String::newString(u8"velocity"),         script::Number::newNumber(msg.getFloatVelocity()));
        }

        if (msg.isController()) {
            obj.set(script::String::newString(u8"controllerNumber"), script::Number::newNumber(msg.getControllerNumber()));
            obj.set(script::String::newString(u8"controllerValue"),  script::Number::newNumber(float(msg.getControllerValue() * (1.0f / 127.0f))));
        }

        try {
            onMidiMessage.call({}, obj);
        } catch (const script::Exception& e) {
            console.postMessage("*** onMidiMessage Exception ***");
            console.postMessage(e.what());
            console.postMessage(e.stacktrace());
        }
    }
}

void EngineProxy::callOnMidiMessage(const tonewheel::MidiMessage& msg)
{
    script::EngineScope scope(scriptEngine.get());

    script::Local<script::Value> f{ scriptEngine->get("onMidiMessage") };

    if (f.isFunction()) {
        auto onMidiMessage{ f.asFunction() };

        auto obj{ script::Object::newObject() };
        obj.set(script::String::newString(u8"noteOn"),           script::Boolean::newBoolean(msg.isNoteOn()));
        obj.set(script::String::newString(u8"noteOff"),          script::Boolean::newBoolean(msg.isNoteOff()));
        obj.set(script::String::newString(u8"controller"),       script::Boolean::newBoolean(msg.isController()));

        if (msg.isNoteOn() || msg.isNoteOff()) {
            obj.set(script::String::newString(u8"noteNumber"),       script::Number::newNumber(msg.getNoteNumber()));
            obj.set(script::String::newString(u8"velocity"),         script::Number::newNumber(msg.getVelocityAsFloat()));
        }

        if (msg.isController()) {
            obj.set(script::String::newString(u8"controllerNumber"), script::Number::newNumber(msg.getControllerNumber()));
            obj.set(script::String::newString(u8"controllerValue"),  script::Number::newNumber(float(msg.getControllerValueAsFloat())));
        }

        try {
            onMidiMessage.call({}, obj);
        } catch (const script::Exception& e) {
            console.postMessage("*** onMidiMessage Exception ***");
            console.postMessage(e.what());
            console.postMessage(e.stacktrace());
        }
    }
}
