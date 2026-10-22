#include "PluginProcessor.h"
#include "flow88/state/FlowParameterIDs.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using Processor = juce::AudioProcessor;
using Buffer = juce::AudioBuffer<float>;
namespace ids = flow88::state::paramIds;
std::function<std::unique_ptr<Processor>()> makeProcessor;
void check(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}
juce::RangedAudioParameter& parameter(Processor& p, const char* id) {
  const std::vector<std::pair<std::string, juce::String>> names{
    {ids::inputTrimDb,"Input Trim"},{ids::outputTrimDb,"Output Trim"},
    {ids::toneTiltDb,"Tone"},{ids::punchPct,"Punch"},{ids::stereoWidthPct,"Width"},
    {ids::lowEndFocusPct,"Low-End Focus"},{ids::targetLufs,"Target LUFS"},
    {ids::attitudePct,"Attitude"},{ids::bypass,"Bypass"},{ids::oversampling,"Oversampling"},
    {ids::limiterCeilingDbTP,"Limiter Ceiling"},{ids::clipperAmountPct,"Clipper Amount"},
    {ids::resonanceControlPct,"Resonance Control"},{ids::referenceModeEnabled,"Reference Mode"},
    {ids::subgenre,"Subgenre"},{ids::style,"Style"}};
  juce::String name=id;
  for(const auto& entry:names) if(entry.first==id) name=entry.second;
  for (auto* candidate : p.getParameters())
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(candidate))
      if (ranged->getName(100) == name || ranged->paramID == id) return *ranged;
  throw std::runtime_error(std::string("Missing parameter: ") + id);
}
void set(Processor& p, const char* id, float value) {
  auto& param = parameter(p, id);
  param.setValueNotifyingHost(param.convertTo0to1(value));
}
void prepare(Processor& p, double rate, int block) {
  p.setRateAndBufferSizeDetails(rate, block);
  p.prepareToPlay(rate, block);
}
Buffer signal(int count, double rate, float amplitude = 0.2f, double frequency = 997.0) {
  Buffer b(2, count);
  for (int i=0; i<count; ++i) {
    b.setSample(0, i, amplitude * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi*frequency*i/rate + 0.3)));
    b.setSample(1, i, amplitude * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi*frequency*1.73*i/rate + 1.1)));
  }
  return b;
}
Buffer render(Processor& p, const Buffer& input, int block) {
  Buffer result(input.getNumChannels(), input.getNumSamples());
  juce::MidiBuffer midi;
  for (int offset=0; offset<input.getNumSamples(); offset+=block) {
    const int count = juce::jmin(block, input.getNumSamples()-offset);
    Buffer b(input.getNumChannels(), count);
    for (int ch=0; ch<b.getNumChannels(); ++ch) b.copyFrom(ch, 0, input, ch, offset, count);
    p.processBlock(b, midi);
    for (int ch=0; ch<b.getNumChannels(); ++ch) result.copyFrom(ch, offset, b, ch, 0, count);
  }
  return result;
}
double rms(const Buffer& b, int start=1024) {
  double energy=0; int n=0;
  for (int ch=0; ch<b.getNumChannels(); ++ch)
    for (int i=start; i<b.getNumSamples(); ++i) { energy+=std::pow(b.getSample(ch,i),2); ++n; }
  return std::sqrt(energy/juce::jmax(1,n));
}
double difference(const Buffer& a, const Buffer& b, int start=1024) {
  double energy=0; int n=0;
  for (int ch=0; ch<a.getNumChannels(); ++ch)
    for (int i=start; i<a.getNumSamples(); ++i) { energy+=std::pow(a.getSample(ch,i)-b.getSample(ch,i),2); ++n; }
  return std::sqrt(energy/juce::jmax(1,n));
}
void finite(const Buffer& b) {
  for (int ch=0; ch<b.getNumChannels(); ++ch)
    for (int i=0; i<b.getNumSamples(); ++i)
      check(std::isfinite(b.getSample(ch,i)), "Non-finite audio output");
}
void lifecycle() {
  for (int i=0; i<5; ++i) {
    auto p=makeProcessor(); prepare(*p,48000,512);
    auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p->createEditorIfNeeded());
    check(editor!=nullptr, "Editor did not open");
    auto out=render(*p,signal(4096,48000),512); finite(out);
    editor.reset(); p->releaseResources();
  }
}
void state() {
  // Save the current controls without switching A/B first. The previous test only
  // covered snapshots that had already been captured by switching slots.
  for (int genre=0; genre<5; ++genre) for (int style=0; style<3; ++style) {
    auto p=makeProcessor();
    if (auto* direct=dynamic_cast<Flow88MasterAudioProcessor*>(p.get())) {
      direct->setSubgenreIndex(genre); direct->setStyleIndex(style);
    } else { set(*p,ids::subgenre,static_cast<float>(genre)); set(*p,ids::style,static_cast<float>(style)); }
    set(*p,ids::toneTiltDb,2.31f); set(*p,ids::outputTrimDb,-4.17f);
    set(*p,ids::oversampling,static_cast<float>(genre%3));
    std::vector<float> values;
    for (auto* param:p->getParameters()) values.push_back(param->getValue());
    juce::MemoryBlock data; p->getStateInformation(data); p.reset();
    auto restored=makeProcessor(); restored->setStateInformation(data.getData(),static_cast<int>(data.getSize()));
    check(values.size()==static_cast<size_t>(restored->getParameters().size()),"Parameter count changed");
    for (int i=0; i<restored->getParameters().size(); ++i)
      check(std::abs(restored->getParameters()[i]->getValue()-values[static_cast<size_t>(i)])<0.0001f,
            "Current control lost on reopen: " + restored->getParameters()[i]->getName(100).toStdString());
  }
}
void audioMatrix() {
  for (double rate:{44100.0,48000.0,88200.0,96000.0,192000.0})
    for (int block:{1,17,64,257,512,1024}) for (int os=0; os<3; ++os) {
      auto p=makeProcessor(); set(*p,ids::oversampling,static_cast<float>(os)); prepare(*p,rate,block);
      auto in=signal(4096,rate); auto out=render(*p,in,block); finite(out);
      check(out.getRMSLevel(0,1024,3072)>0.001f && out.getRMSLevel(1,1024,3072)>0.001f,"Stereo output missing");
      double delta=0; for(int i=1024;i<4096;++i) delta+=std::abs(out.getSample(0,i)-out.getSample(1,i));
      check(delta>1.0,"Stereo channels collapsed"); p->releaseResources();
    }
  // A host may deliver a block larger than the prepare hint, or a zero-length block.
  auto p=makeProcessor(); prepare(*p,48000,64);
  juce::MidiBuffer midi; Buffer zero(2,0); p->processBlock(zero,midi);
  Buffer large=signal(2049,48000); p->processBlock(large,midi); finite(large);
  std::cout << "90 stereo rate/block/oversampling combinations; zero and oversized blocks\n";
}
Buffer controlled(const char* id,float value,const Buffer& in) {
  auto p=makeProcessor(); set(*p,ids::oversampling,0); set(*p,id,value); prepare(*p,48000,257);
  return render(*p,in,257);
}
void controls() {
  auto in=signal(16384,48000,0.35f,3100);
  struct Range {const char* id; float a,b;};
  for (const auto& r:std::vector<Range>{{ids::inputTrimDb,-6,6},{ids::outputTrimDb,-6,0},
      {ids::toneTiltDb,-3,3},{ids::punchPct,0,100},{ids::stereoWidthPct,50,150},
      {ids::lowEndFocusPct,0,100},{ids::attitudePct,0,100},{ids::targetLufs,-16,-7},
      {ids::clipperAmountPct,0,100},{ids::resonanceControlPct,0,100},{ids::limiterCeilingDbTP,-1.5f,-0.1f}}) {
    auto a=controlled(r.id,r.a,in), b=controlled(r.id,r.b,in);
    const auto delta=difference(a,b);
    std::cout<<r.id<<" audio RMS difference="<<delta<<"\n";
    check(delta>0.00001,std::string("Control had no audible signal effect: ")+r.id);
    if (std::string(r.id)==ids::outputTrimDb)
      check(std::abs(20*std::log10(rms(b)/rms(a))-6.0)<0.1,"Output trim not 6 dB on low-level material");
  }
}
void bypass() {
  auto p=makeProcessor(); set(*p,ids::bypass,1); set(*p,ids::inputTrimDb,12); set(*p,ids::outputTrimDb,12);
  set(*p,ids::oversampling,0); prepare(*p,48000,257);
  auto in=signal(8192,48000); auto out=render(*p,in,257);
  check(difference(in,out,0)<1.0e-7,"Bypass changes gain or samples");
  // Reproduce the non-unity A/B correction before enabling bypass.
  if (auto* direct=dynamic_cast<Flow88MasterAudioProcessor*>(p.get())) {
    set(*p,ids::bypass,0); set(*p,ids::inputTrimDb,0); set(*p,ids::outputTrimDb,0);
    render(*p,signal(288000,48000),512); direct->switchABSlot(1);
    set(*p,ids::outputTrimDb,-6); render(*p,signal(288000,48000),512); direct->switchABSlot(0);
    check(std::abs(direct->buildDisplayState(0).abLevelMatchGainDb)>0.5f,"A/B fixture did not create gain correction");
    set(*p,ids::bypass,1); out=render(*p,in,257);
    check(difference(in,out,0)<1.0e-7,"A/B correction leaks into bypass");
  }
}
void extremes() {
  for (int os=0;os<3;++os) for (float end:{0.0f,1.0f}) {
    auto p=makeProcessor();
    for (auto* param:p->getParameters()) param->setValueNotifyingHost(end);
    set(*p,ids::bypass,0); set(*p,ids::referenceModeEnabled,0); set(*p,ids::oversampling,static_cast<float>(os));
    prepare(*p,48000,64);
    Buffer silence(2,4096); silence.clear(); auto quiet=render(*p,silence,64);
    check(quiet.getMagnitude(0,4096)<1.0e-7,"Silence generates audio");
    auto loud=signal(8192,48000,8.0f,12000); auto out=render(*p,loud,64); finite(out);
    check(out.getMagnitude(0,8192)<1.25f,"Clipping/extremes produce uncontrolled sample peaks");
  }
}
void latency() {
  flow88::dsp::FlowMasteringChain chain; chain.prepare(48000,512,2);
  for(int os=0;os<3;++os) {
    auto p=makeProcessor(); set(*p,ids::oversampling,static_cast<float>(os)); prepare(*p,48000,512);
    // Compare to the actual oversampling filter delay, not a hard-coded host value.
    float expected=0;
    if(os>0) { juce::dsp::Oversampling<float> filter(2,static_cast<size_t>(os),juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);
      filter.initProcessing(512); expected=filter.getLatencyInSamples(); }
    std::cout<<"oversampling="<<os<<" expected latency="<<expected<<" reported="<<p->getLatencySamples()<<"\n";
    check(p->getLatencySamples()==static_cast<int>(std::round(expected)),"Oversampling latency not reported");
    set(*p,ids::bypass,1); Buffer impulse(2,512); impulse.clear(); impulse.setSample(0,0,0.25f); impulse.setSample(1,0,-0.25f);
    auto out=render(*p,impulse,512);
    const int delay=p->getLatencySamples();
    check(std::abs(out.getSample(0,delay)-0.25f)<1.0e-7f && std::abs(out.getSample(1,delay)+0.25f)<1.0e-7f,"Bypass does not preserve reported latency");
  }
}
// Independent band-limited reconstruction. This deliberately does not use the
// plugin's own meter or limiter oversampler as the true-peak oracle.
double reconstructedPeak(const Buffer& b) {
  constexpr int radius=32, factor=16; double peak=0;
  for (int ch=0;ch<2;++ch) for(int phase=0;phase<factor;++phase) {
    double weights[2*radius]; double sum=0;
    const double fraction=static_cast<double>(phase)/factor;
    for(int tap=-radius+1;tap<=radius;++tap) {
      const double x=fraction-tap;
      const double sinc=std::abs(x)<1.0e-12?1.0:std::sin(juce::MathConstants<double>::pi*x)/(juce::MathConstants<double>::pi*x);
      const double window=0.5+0.5*std::cos(juce::MathConstants<double>::pi*x/radius);
      weights[tap+radius-1]=sinc*window; sum+=sinc*window;
    }
    for(int i=2048;i<4096;++i) {
      double value=0;
      for(int tap=-radius+1;tap<=radius;++tap) value+=weights[tap+radius-1]*b.getSample(ch,i+tap);
      peak=juce::jmax(peak,std::abs(value/sum));
    }
  }
  return peak;
}
void truePeak() {
  bool passed=true;
  for(double rate:{44100.0,48000.0,96000.0}) for(int os=0;os<3;++os) {
    auto p=makeProcessor(); set(*p,ids::oversampling,static_cast<float>(os));
    set(*p,ids::inputTrimDb,12); set(*p,ids::outputTrimDb,12); set(*p,ids::attitudePct,100);
    set(*p,ids::clipperAmountPct,100); set(*p,ids::limiterCeilingDbTP,-1.0f); prepare(*p,rate,257);
    auto in=signal(8192,rate,4.0f,rate/4); auto out=render(*p,in,257); finite(out);
    const double db=20*std::log10(reconstructedPeak(out));
    std::cout<<"rate="<<rate<<" oversampling="<<os<<" sample peak="<<20*std::log10(out.getMagnitude(0,out.getNumSamples()))<<" dBFS; independent 16x reconstructed peak="<<db<<" dBTP (ceiling -1.0)\n";
    passed=passed && db<=-0.9; // 0.1 dB measurement tolerance
  }
  check(passed,"True-peak ceiling exceeded on driven intersample stress signal");
}
}
int main(int argc,char** argv) {
  juce::ScopedJuceInitialiser_GUI initialise;
  try {
    check(argc>=2,"Expected test name and optional VST3 path");
    juce::AudioPluginFormatManager formats;
    if(argc>2) {
      formats.addFormat(std::make_unique<juce::VST3PluginFormat>());
      juce::OwnedArray<juce::PluginDescription> descriptions;
      formats.getFormat(0)->findAllTypesForFile(descriptions,argv[2]);
      check(descriptions.size()==1,"Expected exactly one loadable VST3 class");
      const auto description=*descriptions[0];
      makeProcessor=[&formats,description] {juce::String error; auto p=formats.createPluginInstance(description,48000,512,error);
        check(p!=nullptr,"VST3 failed to load: "+error.toStdString()); return std::unique_ptr<Processor>(std::move(p));};
    } else makeProcessor=[] {return std::make_unique<Flow88MasterAudioProcessor>();};
    const std::string name=argv[1];
    if(name=="lifecycle") lifecycle(); else if(name=="state") state(); else if(name=="audio") audioMatrix();
    else if(name=="controls") controls(); else if(name=="bypass") bypass(); else if(name=="extremes") extremes();
    else if(name=="latency") latency(); else if(name=="truepeak") truePeak(); else check(false,"Unknown test");
    std::cout<<"PASS "<<name<<(argc>2?" (loaded VST3)":" (processor)")<<"\n"; return 0;
  } catch(const std::exception& e) {std::cerr<<"FAIL "<<argv[1]<<": "<<e.what()<<"\n"; return 1;}
}
