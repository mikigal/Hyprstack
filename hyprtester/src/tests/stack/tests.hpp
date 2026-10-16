#pragma once
#include "../../shared.hpp"

#include <memory>
#include <vector>

// Hyprstack: stacking window manager tests
inline std::vector<std::shared_ptr<CTestCase>> stackTestCases;

#ifndef INCLUDED_FROM_MAIN
#define TEST_GROUP_NAME         "stack"
#define GROUP_TEST_CASE_STORAGE stackTestCases
#endif // !defined(INCLUDED_FROM_MAIN)
