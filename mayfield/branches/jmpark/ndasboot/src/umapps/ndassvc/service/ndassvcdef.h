#pragma once
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

class CNdasDevice;
class CNdasUnitDevice;
class CNdasLogicalDevice;

typedef boost::shared_ptr<CNdasDevice> CNdasDevicePtr;
typedef boost::shared_ptr<CNdasUnitDevice> CNdasUnitDevicePtr;
typedef boost::shared_ptr<CNdasLogicalDevice> CNdasLogicalDevicePtr;

typedef boost::weak_ptr<CNdasDevice>        CNdasDeviceWeakPtr;
typedef boost::weak_ptr<CNdasUnitDevice>    CNdasUnitDeviceWeakPtr;
typedef boost::weak_ptr<CNdasLogicalDevice> CNdasLogicalDeviceWeakPtr;

typedef std::vector<CNdasLogicalDevicePtr> CNdasLogicalDeviceVector;
typedef std::vector<CNdasDevicePtr>        CNdasDeviceVector;
typedef std::vector<CNdasUnitDevicePtr>    CNdasUnitDeviceVector;

typedef std::vector<CNdasLogicalDeviceWeakPtr> CNdasLogicalDeviceWeakVector;
typedef std::vector<CNdasDeviceWeakPtr>        CNdasDeviceWeakVector;
typedef std::vector<CNdasUnitDeviceWeakPtr>    CNdasUnitDeviceWeakVector;

const CNdasDevicePtr        CNdasDeviceNullPtr        = CNdasDevicePtr();
const CNdasUnitDevicePtr    CNdasUnitDeviceNullPtr    = CNdasUnitDevicePtr();
const CNdasLogicalDevicePtr CNdasLogicalDeviceNullPtr = CNdasLogicalDevicePtr();
