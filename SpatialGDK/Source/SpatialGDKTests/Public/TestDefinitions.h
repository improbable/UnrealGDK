// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Misc/AutomationTest.h"

#define TEST(ModuleName, ComponentName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestName, "SpatialGDK."#ModuleName"."#ComponentName"."#TestName, EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter) \
	bool TestName::RunTest(const FString& Parameters)

#define COMPLEX_TEST(ModuleName, ComponentName, TestName) \
	IMPLEMENT_COMPLEX_AUTOMATION_TEST(TestName, "SpatialGDK."#ModuleName"."#ComponentName"."#TestName, EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter) \
	bool TestName::RunTest(const FString& Parameters)

#define DEFINE_LATENT_COMMAND(CommandName) \
	DEFINE_LATENT_AUTOMATION_COMMAND(CommandName); \
	bool CommandName::Update()

#define DEFINE_LATENT_COMMAND_ONE_PARAMETER(CommandName, ParamType1, Param1) \
	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(CommandName, ParamType1, Param1); \
	bool CommandName::Update()

#define DEFINE_LATENT_COMMAND_TWO_PARAMETERS(CommandName, ParamType1, Param1, ParamType2, Param2) \
	DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(CommandName, ParamType1, Param1, ParamType2, Param2); \
	bool CommandName::Update()
/*
Dummy objects
	are passed around but never actually used. Usually they are just used to fill parameter lists.

Fake objects
	actually have working implementations, but usually take some shortcut which makes them not suitable for production (an InMemoryTestDatabase is a good example).

Stubs
	provide canned answers to calls made during the test, usually not responding at all to anything outside what's programmed in for the test.

Spies
	are stubs that also record some information based on how they were called.
	One form of this might be an email service that records how many messages it was sent.

Mocks
	are pre-programmed with expectations which form a specification of the calls they are expected to receive.
	They can throw an exception if they receive a call they don't expect and are checked during verification to ensure they got all the calls they were expecting.
*/
