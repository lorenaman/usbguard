//
// Copyright (C) 2015 Red Hat, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Authors: Daniel Kopecek <dkopecek@redhat.com>
//
#include "DevicePrivate.hpp"
#include "LoggerPrivate.hpp"
#include <mutex>
#include <sodium.h>

namespace usbguard {
  DevicePrivate::DevicePrivate(Device& p_instance)
    : _p_instance(p_instance)
  {
    (void)_p_instance;
    _id = Rule::DefaultID;
    _target = Rule::Target::Unknown;
  }

  DevicePrivate::DevicePrivate(Device& p_instance, const DevicePrivate& rhs)
    : _p_instance(p_instance)
  {
    *this = rhs;
  }

  DevicePrivate::DevicePrivate(Device& p_instance, const Rule& device_rule)
    : _p_instance(p_instance)
  {
    // TODO: Check that the device_rule is of type "Device"

    _id = device_rule.getRuleID();
    _target = device_rule.getTarget();
    _name = device_rule.getName();
    _device_id = device_rule.getDeviceID();
    _serial_number = device_rule.getSerial();
    _port = device_rule.getViaPort();
    _interface_types = device_rule.attributeWithInterface().values();
  }

  const DevicePrivate& DevicePrivate::operator=(const DevicePrivate& rhs)
  {
    _id = rhs._id;
    _target = rhs._target;
    _name = rhs._name;
    _device_id = rhs._device_id;
    _serial_number = rhs._serial_number;
    _port = rhs._port;
    _interface_types = rhs._interface_types;

    return *this;
  }
  
  std::mutex& DevicePrivate::refDeviceMutex()
  {
    return _mutex;
  }

  Pointer<Rule> DevicePrivate::getDeviceRule(const bool include_port)
  {
    Pointer<Rule> device_rule = makePointer<Rule>();
    std::unique_lock<std::mutex> device_lock(refDeviceMutex());

    logger->trace("Generating rule for device {}@{} (name={}); include_port={}",
		  _device_id.toString(), _port, _name, include_port);

    device_rule->setRuleID(_id);
    device_rule->setTarget(_target);
    device_rule->setDeviceID(_device_id);
    device_rule->setSerial(_serial_number);

    if (include_port) {
      device_rule->setViaPort(_port);
    }

    device_rule->attributeWithInterface().set(getInterfaceTypes(), Rule::SetOperator::Equals);
    device_rule->setName(_name);
    device_rule->setHash(getHash(/*include_port=*/false));
    
    return device_rule;
  }

  String DevicePrivate::getHash(const bool include_port) const
  {
    unsigned char hash[crypto_generichash_BYTES_MIN];
    crypto_generichash_state state;

    crypto_generichash_init(&state, NULL, 0, sizeof hash);

    const String vendor_id = _device_id.getVendorID();
    const String product_id = _device_id.getProductID();

    if (vendor_id.empty() || product_id.empty()) {
      throw std::runtime_error("Cannot compute device hash: vendor and/or product id values not available");
    }

    for (auto field : { &_name, &vendor_id, &product_id, &_serial_number }) {
      /* Update the hash value */
      crypto_generichash_update(&state, (const uint8_t *)field->c_str(), field->size());
    }

    /* TODO: hash the descriptor data */

    /* Finalize the hash value */
    crypto_generichash_final(&state, hash, sizeof hash);

    /* Binary => Hex string conversion */
    const size_t hexlen = crypto_generichash_BYTES_MIN * 2 + 1;
    char hexval[hexlen];
    sodium_bin2hex(hexval, hexlen, hash, sizeof hash);

    const std::string hash_string(hexval, hexlen - 1);
    return hash_string;
  }

  void DevicePrivate::setID(uint32_t id)
  {
    _id = id;
  }

  uint32_t DevicePrivate::getID() const
  {
    return _id;
  }

  void DevicePrivate::setTarget(Rule::Target target)
  {
    _target = target;
  }

  Rule::Target DevicePrivate::getTarget() const
  {
    return _target;
  }

  void DevicePrivate::setName(const String& name)
  {
    if (name.size() > USB_GENERIC_STRING_MAX_LENGTH) {
      throw std::runtime_error("setDeviceName: value size out-of-range");
    }
    _name = name;
  }

  const String& DevicePrivate::getName() const
  {
    return _name;
  }

  void DevicePrivate::setDeviceID(const USBDeviceID& device_id)
  {
    _device_id = device_id;
  }

  const USBDeviceID& DevicePrivate::getDeviceID() const
  {
    return _device_id;
  }

  void DevicePrivate::setPort(const String& port)
  {
    _port = port;
  }

  const String& DevicePrivate::getPort() const
  {
    return _port;
  }

  void DevicePrivate::setSerial(const String& serial_number)
  {
    _serial_number = serial_number;
  }

  const String& DevicePrivate::getSerial() const
  {
    return _serial_number;
  }

  std::vector<USBInterfaceType>& DevicePrivate::refMutableInterfaceTypes()
  {
    return _interface_types;
  }

  const std::vector<USBInterfaceType>& DevicePrivate::getInterfaceTypes() const
  {
    return _interface_types;
  }

    /* FIXME: move this to USBDeviceID
    if (vendor_id.size() > USB_VID_STRING_MAX_LENGTH) {
      throw std::runtime_error("setVendorID: value size out-of-range");
    }
    */
/*
    if (product_id.size() > USB_PID_STRING_MAX_LENGTH) {
      throw std::runtime_error("setProductID: value size out-of-range");
    }

    if (port.size() > USB_PORT_STRING_MAX_LENGTH) {
      throw std::runtime_error("setDevicePort: value size out-of-range");
    }
*/
/*
    if (serial_number.size() > USB_GENERIC_STRING_MAX_LENGTH) {
      throw std::runtime_error("setSerialNumber: value size out-of-range");
    }
    */

  void DevicePrivate::loadDeviceDescriptor(USBDescriptorParser* parser, const USBDescriptor* const descriptor)
  {
    if (parser->haveDescriptor(USB_DESCRIPTOR_TYPE_DEVICE)) {
      throw std::runtime_error("Invalid descriptor data: multiple device descriptors for one device");
    }
    _interface_types.clear();
    return;
  }

  void DevicePrivate::loadConfigurationDescriptor(USBDescriptorParser* parser, const USBDescriptor* const descriptor)
  {
    if (!parser->haveDescriptor(USB_DESCRIPTOR_TYPE_DEVICE)) {
      throw std::runtime_error("Invalid descriptor data: missing parent device descriptor while loading configuration");
    }
    /*
     * Clean the descriptor state. There shouldn't be any Interface or Endpoint
     * descriptors while loading.
     */
    parser->delDescriptor(USB_DESCRIPTOR_TYPE_INTERFACE);
    parser->delDescriptor(USB_DESCRIPTOR_TYPE_ENDPOINT);

    return;
  }

  void DevicePrivate::loadInterfaceDescriptor(USBDescriptorParser* parser, const USBDescriptor* const descriptor)
  {
    if (!parser->haveDescriptor(USB_DESCRIPTOR_TYPE_CONFIGURATION)) {
      throw std::runtime_error("Invalid descriptor data: missing parent configuration descriptor while loading interface");
    }

    const USBInterfaceType interface_type(*reinterpret_cast<const USBInterfaceDescriptor*>(descriptor));
    _interface_types.push_back(interface_type);

    return;
  }

  void DevicePrivate::loadEndpointDescriptor(USBDescriptorParser* parser, const USBDescriptor* const descriptor)
  {
    if (!parser->haveDescriptor(USB_DESCRIPTOR_TYPE_INTERFACE)) {
      throw std::runtime_error("Invalid descriptor data: missing parent interface descriptor while loading endpoint");
    }
    return;
  }
} /* namespace usbguard */
