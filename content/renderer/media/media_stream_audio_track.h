// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_H_

#include <memory>

#include "base/atomicops.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/media/media_stream_audio_deliverer.h"
#include "content/renderer/media/media_stream_track.h"

namespace content {

class MediaStreamAudioSink;
class MediaStreamAudioSource;

// Provides the part of the audio pipeline delivering audio from a
// MediaStreamAudioSource to one or more MediaStreamAudioSinks. An instance of
// this class is owned by blink::WebMediaStreamTrack, and clients should use
// From() to gain access to a MediaStreamAudioTrack.
class CONTENT_EXPORT MediaStreamAudioTrack : public MediaStreamTrack {
 public:
  explicit MediaStreamAudioTrack(bool is_local_track);

  ~MediaStreamAudioTrack() override;

  // Returns the MediaStreamAudioTrack instance owned by the given blink |track|
  // or null.
  static MediaStreamAudioTrack* From(const blink::WebMediaStreamTrack& track);

  // Provides a weak reference to this MediaStreamAudioTrack which is
  // invalidated when Stop() is called. The weak pointer may only be
  // dereferenced on the main thread.
  base::WeakPtr<MediaStreamAudioTrack> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Add a sink to the track. This function will trigger a OnSetFormat()
  // call on the |sink| before the first chunk of audio is delivered.
  void AddSink(MediaStreamAudioSink* sink);

  // Remove a sink from the track. When this method returns, the sink's
  // OnSetFormat() and OnData() methods will not be called again on any thread.
  void RemoveSink(MediaStreamAudioSink* sink);

  // Returns the output format of the capture source. May return an invalid
  // AudioParameters if the format is not yet available.
  // Called on the main render thread.
  // TODO(tommi): This method appears to only be used by Pepper and in fact
  // does not appear to be necessary there.  We should remove it since it adds
  // to the complexity of all types of audio tracks+source implementations.
  // http://crbug.com/577874
  media::AudioParameters GetOutputFormat() const;

  // Halts the flow of audio data from the source (and to the sinks), and then
  // notifies all sinks of the "ended" state.
  void StopAndNotify(base::OnceClosure callback) final;

  // MediaStreamTrack override.
  void SetEnabled(bool enabled) override;
  void SetContentHint(
      blink::WebMediaStreamTrack::ContentHintType content_hint) override;

  // Returns a unique class identifier. Some subclasses override and use this
  // method to provide safe down-casting to their type.
  virtual void* GetClassIdentifier() const;

 private:
  friend class MediaStreamAudioSource;
  friend class MediaStreamAudioDeliverer<MediaStreamAudioTrack>;

  // Called by MediaStreamAudioSource to notify this track that the flow of
  // audio data has started from the source. |stop_callback| is run by Stop()
  // when the source must halt the flow of audio data to this track.
  void Start(const base::Closure& stop_callback);

  // Called by the MediaStreamAudioDeliverer to notify this track of an audio
  // format change. In turn, all MediaStreamAudioSinks will be notified before
  // the next chunk of audio is delivered to them.
  void OnSetFormat(const media::AudioParameters& params);

  // Called by the MediaStreamAudioDeliverer to deliver audio data to this
  // track, which in turn delivers the audio to one or more
  // MediaStreamAudioSinks. While this track is disabled, silent audio will be
  // delivered to the sinks instead of the content of |audio_bus|.
  void OnData(const media::AudioBus& audio_bus, base::TimeTicks reference_time);

 private:
  // In debug builds, check that all methods that could cause object graph
  // or data flow changes are being called on the main thread.
  base::ThreadChecker thread_checker_;

  // Callback provided to Start() which is run when the audio flow must halt.
  base::Closure stop_callback_;

  // Manages sinks connected to this track and the audio format and data flow.
  MediaStreamAudioDeliverer<MediaStreamAudioSink> deliverer_;

  // While false (0), silent audio is delivered to the sinks.
  base::subtle::Atomic32 is_enabled_;

  // Buffer used to deliver silent audio data while this track is disabled.
  std::unique_ptr<media::AudioBus> silent_bus_;

  // Provides weak pointers that are valid until Stop() is called.
  base::WeakPtrFactory<MediaStreamAudioTrack> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamAudioTrack);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_H_
