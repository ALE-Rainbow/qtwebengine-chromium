/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RealtimeAnalyser_h
#define RealtimeAnalyser_h

#include "core/dom/DOMTypedArray.h"
#include "platform/audio/AudioArray.h"
#include "platform/audio/FFTFrame.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"

namespace blink {

class AudioBus;

class RealtimeAnalyser final {
    WTF_MAKE_NONCOPYABLE(RealtimeAnalyser);
    DISALLOW_NEW();
public:
    RealtimeAnalyser();

    size_t fftSize() const { return m_fftSize; }
    bool setFftSize(size_t);

    unsigned frequencyBinCount() const { return m_fftSize / 2; }

    void setMinDecibels(double k) { m_minDecibels = k; }
    double minDecibels() const { return m_minDecibels; }

    void setMaxDecibels(double k) { m_maxDecibels = k; }
    double maxDecibels() const { return m_maxDecibels; }

    void setSmoothingTimeConstant(double k) { m_smoothingTimeConstant = k; }
    double smoothingTimeConstant() const { return m_smoothingTimeConstant; }

    void getFloatFrequencyData(DOMFloat32Array*, double);
    void getByteFrequencyData(DOMUint8Array*, double);
    void getFloatTimeDomainData(DOMFloat32Array*);
    void getByteTimeDomainData(DOMUint8Array*);

    // The audio thread writes input data here.
    void writeInput(AudioBus*, size_t framesToProcess);

    static const double DefaultSmoothingTimeConstant;
    static const double DefaultMinDecibels;
    static const double DefaultMaxDecibels;

    static const unsigned DefaultFFTSize;
    static const unsigned MinFFTSize;
    static const unsigned MaxFFTSize;
    static const unsigned InputBufferSize;

private:
    // The audio thread writes the input audio here.
    AudioFloatArray m_inputBuffer;
    unsigned m_writeIndex;

    // Input audio is downmixed to this bus before copying to m_inputBuffer.
    RefPtr<AudioBus> m_downMixBus;

    size_t m_fftSize;
    OwnPtr<FFTFrame> m_analysisFrame;
    void doFFTAnalysis();

    // Convert the contents of magnitudeBuffer to byte values, saving the result in |destination|.
    void convertToByteData(DOMUint8Array* destination);

    // Convert magnidue buffer to dB, saving the result in |destination|
    void convertFloatToDb(DOMFloat32Array* destination);

    // doFFTAnalysis() stores the floating-point magnitude analysis data here.
    AudioFloatArray m_magnitudeBuffer;
    AudioFloatArray& magnitudeBuffer() { return m_magnitudeBuffer; }

    // A value between 0 and 1 which averages the previous version of m_magnitudeBuffer with the current analysis magnitude data.
    double m_smoothingTimeConstant;

    // The range used when converting when using getByteFrequencyData().
    double m_minDecibels;
    double m_maxDecibels;

    // Time at which the FFT was last computed.
    double m_lastAnalysisTime;
};

} // namespace blink

#endif // RealtimeAnalyser_h
