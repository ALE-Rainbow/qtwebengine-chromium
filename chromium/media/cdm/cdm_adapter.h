// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_ADAPTER_H_
#define MEDIA_CDM_CDM_ADAPTER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_native_library.h"
#include "base/threading/thread.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_factory.h"
#include "media/base/cdm_promise_adapter.h"
#include "media/base/decryptor.h"
#include "media/base/media_export.h"
#include "media/base/media_keys.h"
#include "media/cdm/api/content_decryption_module.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class AudioFramesImpl;
class CdmAllocator;
class CdmWrapper;

class MEDIA_EXPORT CdmAdapter : public MediaKeys,
                                public CdmContext,
                                public Decryptor,
                                NON_EXPORTED_BASE(public cdm::Host_7),
                                NON_EXPORTED_BASE(public cdm::Host_8) {
 public:
  // Create the CDM using |cdm_path| and initialize it using |key_system| and
  // |cdm_config|. |allocator| is to be used whenever the CDM needs memory
  // and to create VideoFrames. Callbacks will be used for events generated
  // by the CDM. |cdm_created_cb| will be called when the CDM is loaded and
  // initialized.
  static void Create(
      const std::string& key_system,
      const base::FilePath& cdm_path,
      const CdmConfig& cdm_config,
      scoped_ptr<CdmAllocator> allocator,
      const SessionMessageCB& session_message_cb,
      const SessionClosedCB& session_closed_cb,
      const LegacySessionErrorCB& legacy_session_error_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb,
      const CdmCreatedCB& cdm_created_cb);

  // MediaKeys implementation.
  void SetServerCertificate(const std::vector<uint8_t>& certificate,
                            scoped_ptr<SimpleCdmPromise> promise) final;
  void CreateSessionAndGenerateRequest(
      SessionType session_type,
      EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      scoped_ptr<NewSessionCdmPromise> promise) final;
  void LoadSession(SessionType session_type,
                   const std::string& session_id,
                   scoped_ptr<NewSessionCdmPromise> promise) final;
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     scoped_ptr<SimpleCdmPromise> promise) final;
  void CloseSession(const std::string& session_id,
                    scoped_ptr<SimpleCdmPromise> promise) final;
  void RemoveSession(const std::string& session_id,
                     scoped_ptr<SimpleCdmPromise> promise) final;
  CdmContext* GetCdmContext() final;

  // CdmContext implementation.
  Decryptor* GetDecryptor() final;
  int GetCdmId() const final;

  // Decryptor implementation.
  void RegisterNewKeyCB(StreamType stream_type,
                        const NewKeyCB& key_added_cb) final;
  void Decrypt(StreamType stream_type,
               const scoped_refptr<DecoderBuffer>& encrypted,
               const DecryptCB& decrypt_cb) final;
  void CancelDecrypt(StreamType stream_type) final;
  void InitializeAudioDecoder(const AudioDecoderConfig& config,
                              const DecoderInitCB& init_cb) final;
  void InitializeVideoDecoder(const VideoDecoderConfig& config,
                              const DecoderInitCB& init_cb) final;
  void DecryptAndDecodeAudio(const scoped_refptr<DecoderBuffer>& encrypted,
                             const AudioDecodeCB& audio_decode_cb) final;
  void DecryptAndDecodeVideo(const scoped_refptr<DecoderBuffer>& encrypted,
                             const VideoDecodeCB& video_decode_cb) final;
  void ResetDecoder(StreamType stream_type) final;
  void DeinitializeDecoder(StreamType stream_type) final;

  // cdm::Host_7 and cdm::Host_8 implementation.
  cdm::Buffer* Allocate(uint32_t capacity) override;
  void SetTimer(int64_t delay_ms, void* context) override;
  cdm::Time GetCurrentWallTime() override;
  void OnResolveNewSessionPromise(uint32_t promise_id,
                                  const char* session_id,
                                  uint32_t session_id_size) override;
  void OnResolvePromise(uint32_t promise_id) override;
  void OnRejectPromise(uint32_t promise_id,
                       cdm::Error error,
                       uint32_t system_code,
                       const char* error_message,
                       uint32_t error_message_size) override;
  void OnSessionMessage(const char* session_id,
                        uint32_t session_id_size,
                        cdm::MessageType message_type,
                        const char* message,
                        uint32_t message_size,
                        const char* legacy_destination_url,
                        uint32_t legacy_destination_url_size) override;
  void OnSessionKeysChange(const char* session_id,
                           uint32_t session_id_size,
                           bool has_additional_usable_key,
                           const cdm::KeyInformation* keys_info,
                           uint32_t keys_info_count) override;
  void OnExpirationChange(const char* session_id,
                          uint32_t session_id_size,
                          cdm::Time new_expiry_time) override;
  void OnSessionClosed(const char* session_id,
                       uint32_t session_id_size) override;
  void OnLegacySessionError(const char* session_id,
                            uint32_t session_id_size,
                            cdm::Error error,
                            uint32_t system_code,
                            const char* error_message,
                            uint32_t error_message_size) override;
  void SendPlatformChallenge(const char* service_id,
                             uint32_t service_id_size,
                             const char* challenge,
                             uint32_t challenge_size) override;
  void EnableOutputProtection(uint32_t desired_protection_mask) override;
  void QueryOutputProtectionStatus() override;
  void OnDeferredInitializationDone(cdm::StreamType stream_type,
                                    cdm::Status decoder_status) override;
  cdm::FileIO* CreateFileIO(cdm::FileIOClient* client) override;

 private:
  CdmAdapter(const std::string& key_system,
             const CdmConfig& cdm_config,
             scoped_ptr<CdmAllocator> allocator,
             const SessionMessageCB& session_message_cb,
             const SessionClosedCB& session_closed_cb,
             const LegacySessionErrorCB& legacy_session_error_cb,
             const SessionKeysChangeCB& session_keys_change_cb,
             const SessionExpirationUpdateCB& session_expiration_update_cb);
  ~CdmAdapter() final;

  // Load the CDM using |cdm_path| and initialize it. |promise| is resolved if
  // the CDM is successfully loaded and initialized, rejected otherwise.
  void Initialize(const base::FilePath& cdm_path,
                  scoped_ptr<media::SimpleCdmPromise> promise);

  // Create an instance of the |key_system| CDM contained in |cdm_path|.
  // Caller owns the returned pointer. On error (unable to load, does not
  // support |key_system|, does not support an supported interface, etc.)
  // NULL will be returned.
  CdmWrapper* CreateCdmInstance(const std::string& key_system,
                                const base::FilePath& cdm_path);

  // Helper for SetTimer().
  void TimerExpired(void* context);

  // Converts audio data stored in |audio_frames| into individual audio
  // buffers in |result_frames|. Returns true upon success.
  bool AudioFramesDataToAudioFrames(scoped_ptr<AudioFramesImpl> audio_frames,
                                    Decryptor::AudioFrames* result_frames);

  // Keep a reference to the CDM.
  base::ScopedNativeLibrary library_;

  // Used to keep track of promises while the CDM is processing the request.
  CdmPromiseAdapter cdm_promise_adapter_;

  scoped_ptr<CdmWrapper> cdm_;
  std::string key_system_;
  CdmConfig cdm_config_;

  // Callbacks for firing session events.
  SessionMessageCB session_message_cb_;
  SessionClosedCB session_closed_cb_;
  LegacySessionErrorCB legacy_session_error_cb_;
  SessionKeysChangeCB session_keys_change_cb_;
  SessionExpirationUpdateCB session_expiration_update_cb_;

  // Callbacks for deferred initialization.
  DecoderInitCB audio_init_cb_;
  DecoderInitCB video_init_cb_;

  // Callbacks for new keys added.
  NewKeyCB new_audio_key_cb_;
  NewKeyCB new_video_key_cb_;

  // Keep track of video frame natural size from the latest configuration
  // as the CDM doesn't provide it.
  gfx::Size natural_size_;

  // Keep track of audio parameters.
  int audio_samples_per_second_;
  ChannelLayout audio_channel_layout_;

  scoped_ptr<CdmAllocator> allocator_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<CdmAdapter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CdmAdapter);
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_ADAPTER_H_
