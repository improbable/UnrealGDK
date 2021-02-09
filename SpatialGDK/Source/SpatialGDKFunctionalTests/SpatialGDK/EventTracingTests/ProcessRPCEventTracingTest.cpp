// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "ProcessRPCEventTracingTest.h"

AProcessRPCEventTracingTest::AProcessRPCEventTracingTest()
{
	Author = "Matthew Sandford";
	Description = TEXT("Test checking the process RPC trace events have appropriate causes");

	FilterEventNames = { ReceiveRPCEventName, ReceiveOpEventName, ApplyRPCEventName };
	WorkerDefinition = FWorkerDefinition::Server(1);
}

void AProcessRPCEventTracingTest::FinishEventTraceTest()
{
	int EventsTested = 0;
	int EventsFailed = 0;
	for (const auto& Pair : TraceEvents)
	{
		const FString& SpanIdString = Pair.Key;
		const FName& EventName = Pair.Value;

		if (EventName == ReceiveOpEventName)
		{
			continue;
		}

		EventsTested++;

		if (EventName == ApplyRPCEventName)
		{
			if (!CheckEventTraceCause(SpanIdString, { ReceiveRPCEventName }, true))
			{
				EventsFailed++;
			}
		}
		else // EventName == ReceiveRPCEventName
		{
			if (!CheckEventTraceCause(SpanIdString, { ReceiveOpEventName }, true))
			{
				EventsFailed++;
			}
		}
	}

	bool bSuccess = EventsTested > 0 && EventsFailed == 0;
	AssertTrue(bSuccess, FString::Printf(TEXT("Process RPC trace events have the expected causes. Events Tested: %d, Events Failed: %d"),
										 EventsTested, EventsFailed));

	FinishStep();
}
