// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_MANAGER_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_MANAGER_H_

#include <string>
#include <unordered_map>

#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/notifications.mojom.h"
#include "components/signin/core/account_id/account_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/message_center/message_center.h"

namespace arc {

class ArcNotificationItem;

class ArcNotificationManager : public ArcService,
                               public ArcBridgeService::Observer,
                               public NotificationsHost {
 public:
  ArcNotificationManager(ArcBridgeService* bridge_service,
                         const AccountId& main_profile_id);

  ArcNotificationManager(ArcBridgeService* bridge_service,
                         const AccountId& main_profile_id,
                         message_center::MessageCenter* message_center);

  ~ArcNotificationManager() override;

  // ArcBridgeService::Observer implementation:
  void OnNotificationsInstanceReady() override;
  void OnNotificationsInstanceClosed() override;

  // NotificationsHost implementation:
  void OnNotificationPosted(ArcNotificationDataPtr data) override;
  void OnNotificationRemoved(const mojo::String& key) override;
  void OnToastPosted(ArcToastDataPtr data) override;
  void OnToastCancelled(ArcToastDataPtr data) override;

  // Methods called from ArcNotificationItem:
  void SendNotificationRemovedFromChrome(const std::string& key);
  void SendNotificationClickedOnChrome(const std::string& key);
  void SendNotificationButtonClickedOnChrome(
      const std::string& key, int button_index);

 private:
  const AccountId main_profile_id_;
  message_center::MessageCenter* const message_center_;

  using ItemMap =
      std::unordered_map<std::string, scoped_ptr<ArcNotificationItem>>;
  ItemMap items_;

  bool ready_ = false;

  mojo::Binding<arc::NotificationsHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationManager);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_MANAGER_H_
