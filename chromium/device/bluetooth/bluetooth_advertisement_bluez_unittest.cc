// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_advertisement_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_le_advertisement_service_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothAdvertisement;

namespace bluez {

class TestAdvertisementObserver : public BluetoothAdvertisement::Observer {
 public:
  explicit TestAdvertisementObserver(
      scoped_refptr<BluetoothAdvertisement> advertisement)
      : released_(false), advertisement_(advertisement) {
    advertisement_->AddObserver(this);
  }

  ~TestAdvertisementObserver() override {
    advertisement_->RemoveObserver(this);
  }

  // BluetoothAdvertisement::Observer overrides:
  void AdvertisementReleased(BluetoothAdvertisement* advertisement) override {
    released_ = true;
  }

  bool released() { return released_; }

 private:
  bool released_;
  scoped_refptr<BluetoothAdvertisement> advertisement_;

  DISALLOW_COPY_AND_ASSIGN(TestAdvertisementObserver);
};

class BluetoothAdvertisementBlueZTest : public testing::Test {
 public:
  void SetUp() override {
    bluez::BluezDBusManager::Initialize(nullptr /* bus */,
                                        true /* use_dbus_stub */);

    callback_count_ = 0;
    error_callback_count_ = 0;

    last_callback_count_ = 0;
    last_error_callback_count_ = 0;

    last_error_code_ = BluetoothAdvertisement::INVALID_ADVERTISEMENT_ERROR_CODE;

    GetAdapter();
  }

  void TearDown() override {
    observer_.reset();
    // The adapter should outlive the advertisement.
    advertisement_ = nullptr;
    adapter_ = nullptr;
    bluez::BluezDBusManager::Shutdown();
  }

  // Gets the existing Bluetooth adapter.
  void GetAdapter() {
    BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothAdvertisementBlueZTest::GetAdapterCallback,
                   base::Unretained(this)));
    base::MessageLoop::current()->Run();
  }

  // Called whenever BluetoothAdapter is retrieved successfully.
  void GetAdapterCallback(scoped_refptr<BluetoothAdapter> adapter) {
    adapter_ = adapter;
    ASSERT_NE(adapter_.get(), nullptr);
    ASSERT_TRUE(adapter_->IsInitialized());
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running()) {
      base::MessageLoop::current()->QuitWhenIdle();
    }
  }

  scoped_ptr<BluetoothAdvertisement::Data> CreateAdvertisementData() {
    scoped_ptr<BluetoothAdvertisement::Data> data =
        make_scoped_ptr(new BluetoothAdvertisement::Data(
            BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST));
    data->set_service_uuids(
        make_scoped_ptr(new BluetoothAdvertisement::UUIDList()));
    data->set_manufacturer_data(
        make_scoped_ptr(new BluetoothAdvertisement::ManufacturerData()));
    data->set_solicit_uuids(
        make_scoped_ptr(new BluetoothAdvertisement::UUIDList()));
    data->set_service_data(
        make_scoped_ptr(new BluetoothAdvertisement::ServiceData()));
    return data;
  }

  // Creates and registers an advertisement with the adapter.
  scoped_refptr<BluetoothAdvertisement> CreateAdvertisement() {
    // Clear the last advertisement we created.
    advertisement_ = nullptr;

    adapter_->RegisterAdvertisement(
        CreateAdvertisementData(),
        base::Bind(&BluetoothAdvertisementBlueZTest::RegisterCallback,
                   base::Unretained(this)),
        base::Bind(&BluetoothAdvertisementBlueZTest::AdvertisementErrorCallback,
                   base::Unretained(this)));

    message_loop_.RunUntilIdle();
    return advertisement_;
  }

  void UnregisterAdvertisement(
      scoped_refptr<BluetoothAdvertisement> advertisement) {
    advertisement->Unregister(
        base::Bind(&BluetoothAdvertisementBlueZTest::Callback,
                   base::Unretained(this)),
        base::Bind(&BluetoothAdvertisementBlueZTest::AdvertisementErrorCallback,
                   base::Unretained(this)));

    message_loop_.RunUntilIdle();
  }

  void TriggerReleased(scoped_refptr<BluetoothAdvertisement> advertisement) {
    BluetoothAdvertisementBlueZ* adv =
        static_cast<BluetoothAdvertisementBlueZ*>(advertisement.get());
    bluez::FakeBluetoothLEAdvertisementServiceProvider* provider =
        static_cast<bluez::FakeBluetoothLEAdvertisementServiceProvider*>(
            adv->provider());
    provider->Release();
  }

  // Called whenever RegisterAdvertisement is completed successfully.
  void RegisterCallback(scoped_refptr<BluetoothAdvertisement> advertisement) {
    ++callback_count_;
    advertisement_ = advertisement;

    ASSERT_NE(advertisement_.get(), nullptr);
  }

  void AdvertisementErrorCallback(
      BluetoothAdvertisement::ErrorCode error_code) {
    ++error_callback_count_;
    last_error_code_ = error_code;
  }

  // Generic callbacks.
  void Callback() { ++callback_count_; }

  void ErrorCallback() { ++error_callback_count_; }

  void ExpectSuccess() {
    EXPECT_EQ(last_error_callback_count_, error_callback_count_);
    EXPECT_EQ(last_callback_count_ + 1, callback_count_);
    last_callback_count_ = callback_count_;
    last_error_callback_count_ = error_callback_count_;
  }

  void ExpectError(BluetoothAdvertisement::ErrorCode error_code) {
    EXPECT_EQ(last_callback_count_, callback_count_);
    EXPECT_EQ(last_error_callback_count_ + 1, error_callback_count_);
    last_callback_count_ = callback_count_;
    last_error_callback_count_ = error_callback_count_;
    EXPECT_EQ(error_code, last_error_code_);
  }

 protected:
  int callback_count_;
  int error_callback_count_;

  int last_callback_count_;
  int last_error_callback_count_;

  BluetoothAdvertisement::ErrorCode last_error_code_;

  base::MessageLoopForIO message_loop_;

  scoped_ptr<TestAdvertisementObserver> observer_;
  scoped_refptr<BluetoothAdapter> adapter_;
  scoped_refptr<BluetoothAdvertisement> advertisement_;
};

TEST_F(BluetoothAdvertisementBlueZTest, RegisterSucceeded) {
  scoped_refptr<BluetoothAdvertisement> advertisement = CreateAdvertisement();
  ExpectSuccess();
  EXPECT_NE(nullptr, advertisement);

  UnregisterAdvertisement(advertisement);
  ExpectSuccess();
}

TEST_F(BluetoothAdvertisementBlueZTest, DoubleRegisterFailed) {
  scoped_refptr<BluetoothAdvertisement> advertisement = CreateAdvertisement();
  ExpectSuccess();
  EXPECT_NE(nullptr, advertisement);

  // Creating a second advertisement should give us an error.
  scoped_refptr<BluetoothAdvertisement> advertisement2 = CreateAdvertisement();
  ExpectError(BluetoothAdvertisement::ERROR_ADVERTISEMENT_ALREADY_EXISTS);
  EXPECT_EQ(nullptr, advertisement2);
}

TEST_F(BluetoothAdvertisementBlueZTest, DoubleUnregisterFailed) {
  scoped_refptr<BluetoothAdvertisement> advertisement = CreateAdvertisement();
  ExpectSuccess();
  EXPECT_NE(nullptr, advertisement);

  UnregisterAdvertisement(advertisement);
  ExpectSuccess();

  // Unregistering an already unregistered advertisement should give us an
  // error.
  UnregisterAdvertisement(advertisement);
  ExpectError(BluetoothAdvertisement::ERROR_ADVERTISEMENT_DOES_NOT_EXIST);
}

TEST_F(BluetoothAdvertisementBlueZTest, UnregisterAfterReleasedFailed) {
  scoped_refptr<BluetoothAdvertisement> advertisement = CreateAdvertisement();
  ExpectSuccess();
  EXPECT_NE(nullptr, advertisement);

  observer_.reset(new TestAdvertisementObserver(advertisement));
  TriggerReleased(advertisement);
  EXPECT_TRUE(observer_->released());

  // Unregistering an advertisement that has been released should give us an
  // error.
  UnregisterAdvertisement(advertisement);
  ExpectError(BluetoothAdvertisement::ERROR_ADVERTISEMENT_DOES_NOT_EXIST);
}

}  // namespace bluez
