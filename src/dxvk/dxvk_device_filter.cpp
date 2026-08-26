#include "dxvk_device_filter.h"
#include <cctype>
#include <iomanip>
#include <sstream>
#include <utility>

namespace dxvk {
  
  static std::string convertUUID(const uint8_t uuid[VK_UUID_SIZE]) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (size_t i = 0; i < VK_UUID_SIZE; ++i)
      stream << std::setw(2) << static_cast<uint32_t>(uuid[i] & 0xff);
    return stream.str();
  }


  static std::string convertLUID(const uint8_t luid[VK_LUID_SIZE]) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (size_t i = 0; i < VK_LUID_SIZE; ++i)
      stream << std::setw(2) << static_cast<uint32_t>(luid[i] & 0xff);
    return stream.str();
  }


  static std::string normalizeLUID(const std::string& value) {
    if (value.size() != VK_LUID_SIZE * 2)
      return { };

    std::string result = value;

    for (char& c : result) {
      if (!std::isxdigit(static_cast<unsigned char>(c)))
        return { };
      c = char(std::tolower(static_cast<unsigned char>(c)));
    }

    return result;
  }


  DxvkDeviceFilter::DxvkDeviceFilter(
          DxvkDeviceFilterFlags flags,
    const DxvkOptions&          options)
  : m_flags(flags) {
    m_matchDeviceName = env::getEnvVar("DXVK_FILTER_DEVICE_NAME");
    m_matchDeviceUUID = env::getEnvVar("DXVK_FILTER_DEVICE_UUID");
    m_matchDeviceLuid = env::getEnvVar("DXVK_FILTER_DEVICE_LUID");

    if (m_matchDeviceName.empty())
      m_matchDeviceName = options.deviceFilter;

    if (m_matchDeviceLuid.empty())
      m_matchDeviceLuid = options.deviceLuid;

    if (!m_matchDeviceLuid.empty()) {
      auto normalized = normalizeLUID(m_matchDeviceLuid);

      if (normalized.empty()) {
        Logger::warn(str::format(
          "Ignoring invalid DXVK_FILTER_DEVICE_LUID/dxvk.deviceLuid value: ",
          m_matchDeviceLuid));
        m_matchDeviceLuid.clear();
      } else {
        m_matchDeviceLuid = std::move(normalized);
      }
    }

    if (!m_matchDeviceName.empty())
      m_flags.set(DxvkDeviceFilterFlag::MatchDeviceName);

    if (!m_matchDeviceUUID.empty())
      m_flags.set(DxvkDeviceFilterFlag::MatchDeviceUUID);

    if (!m_matchDeviceLuid.empty())
      m_flags.set(DxvkDeviceFilterFlag::MatchDeviceLuid);

    if (m_flags.any(DxvkDeviceFilterFlag::MatchDeviceName,
                    DxvkDeviceFilterFlag::MatchDeviceUUID,
                    DxvkDeviceFilterFlag::MatchDeviceLuid))
      m_flags.clr(DxvkDeviceFilterFlag::SkipCpuDevices);
  }


  DxvkDeviceFilter::~DxvkDeviceFilter() {

  }


  bool DxvkDeviceFilter::testAdapter(DxvkAdapter& adapter) const {
    auto adapterInfo = adapter.info();
    auto driverVersion = DxvkDeviceCapabilities::decodeDriverVersion(adapterInfo.driverId, adapterInfo.driverVersion);

    Logger::info(str::format("Found device: ",
      adapterInfo.deviceName, " (",
      adapterInfo.driverName, " ",
      driverVersion.toString(), ")"));

    if (adapterInfo.luidIsValid)
      Logger::info(str::format("  Device LUID: ", convertLUID(adapterInfo.deviceLuid)));

    std::string compatError;

    if (!adapter.isCompatible(compatError)) {
      Logger::info(str::format("  Skipping: ", compatError));
      return false;
    }

    if (m_flags.test(DxvkDeviceFilterFlag::MatchDeviceName)) {
      if (std::string(adapterInfo.deviceName).find(m_matchDeviceName) == std::string::npos) {
        Logger::info("  Skipping: Device filter");
        return false;
      }
    }

    if (m_flags.test(DxvkDeviceFilterFlag::MatchDeviceUUID)) {
      std::string uuidStr = convertUUID(adapterInfo.deviceUuid);

      if (uuidStr.find(m_matchDeviceUUID) == std::string::npos) {
        Logger::info("  Skipping: UUID filter");
        return false;
      }
    }

    if (m_flags.test(DxvkDeviceFilterFlag::MatchDeviceLuid)) {
      if (!adapterInfo.luidIsValid
       || convertLUID(adapterInfo.deviceLuid) != m_matchDeviceLuid) {
        Logger::info("  Skipping: LUID filter");
        return false;
      }
    }

    if (m_flags.test(DxvkDeviceFilterFlag::SkipCpuDevices)) {
      if (adapterInfo.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
        Logger::info("  Skipping: Software driver");
        return false;
      }
    }

    return true;
  }

}
