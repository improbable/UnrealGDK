// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "ComponentUpdateEventTracingTest.h"

AComponentUpdateEventTracingTest::AComponentUpdateEventTracingTest()
{
	Author = "Matthew Sandford";
	Description = TEXT("Test checking the component update trace events have appropriate causes");

	FilterEventNames = { ComponentUpdateEventName, ReceiveOpEventName, MergeComponentUpdateEventName };
	WorkerDefinition = FWorkerDefinition::Client(1);
}

void AComponentUpdateEventTracingTest::FinishEventTraceTest()
{
	int EventsTested = 0;
	int EventsFailed = 0;
	for (const auto& Pair : TraceEvents)
	{
		const FString& SpanIdString = Pair.Key;
		const FName& EventName = Pair.Value;

		if (EventName != ComponentUpdateEventName)
		{
			continue;
		}

		EventsTested++;

		if (!CheckEventTraceCause(SpanIdString, { ReceiveOpEventName, MergeComponentUpdateEventName }))
		{
			EventsFailed++;
		}
	}

	bool bSuccess = EventsTested > 0 && EventsFailed == 0;
	AssertTrue(bSuccess,
			   FString::Printf(TEXT("Component update trace events have the expected causes. Events Tested: %d, Events Failed: %d"),
							   EventsTested, EventsFailed));

	FinishStep();
}
