// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"

#include "SpatialConstants.h"
#include "Containers/Map.h"

#if TRACE_LIB_ACTIVE
#include "WorkerSDK/improbable/trace.h"
#endif

#include "SpatialLatencyTracer.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialLatencyTracing, Log, All);

class AActor;
class UFunction;
class USpatialGameInstance;

namespace SpatialGDK
{
	struct FOutgoingMessage;
}  // namespace SpatialGDK

using TraceKey = int32;

UCLASS()
class SPATIALGDK_API USpatialLatencyTracer : public UObject
{
	GENERATED_BODY()

public:

	//////////////////////////////////////////////////////////////////////////
	//
	// USpatialLatencyTracer allows for tracing of gameplay events across multiple workers, from their user
	// instigation, to their observed results. Each of these multi-worker events are tracked through `traces`
	// which allow the user to see collected timings of these events in a single location. Key timings related
	// to these events are logged throughout the Unreal GDK networking stack. This API makes the assumption
	// that the distributed workers have had their clocks synced by some time syncing protocol (eg. NTP). To
	// give accurate timings, the trace payload is embedded directly within the relevant networking component
	// updates.
	//
	// These timings are logged to Google's Stackdriver (https://cloud.google.com/stackdriver/)
	//
	// Setup:
	// 1. Setup a Google project with access to Stackdriver.
	// 2. Create and download a service-account certificate
	// 3. Set GOOGLE_APPLICATION_CREDENTIALS to certificate path
	// 4. Set GRPC_DEFAULT_SSL_ROOTS_FILE_PATH to your `roots.pem` gRPC path
	//
	// Usage:
	// 1. Register your Google's project id with `RegisterProject`
	// 2. Start a latency trace using `BeginLatencyTrace` tagging it against an Actor's RPC
	// 3. During the execution of the tagged RPC either;
	//		- continue the trace using `ContinueLatencyTrace`, again tagging it against another Actor's RPC
	//		- or end the trace using `EndLatencyTrace`
	//
	//////////////////////////////////////////////////////////////////////////

	USpatialLatencyTracer();

	// Front-end exposed, allows users to register, start, continue, and end traces

	// Call with your google project id. This must be called before latency trace calls are made
	UFUNCTION(BlueprintCallable, Category = "SpatialOS", meta = (WorldContext = "WorldContextObject"))
	static void RegisterProject(UObject* WorldContextObject, const FString& ProjectId);

	// Start a latency trace. This will start the latency timer and attach it to a specific RPC.
	UFUNCTION(BlueprintCallable, Category = "SpatialOS", meta = (WorldContext = "WorldContextObject"))
	static bool BeginLatencyTrace(UObject* WorldContextObject, const AActor* Actor, const FString& FunctionName, const FString& TraceDesc);

	// Hook into an existing latency trace, and pipe the trace to another outgoing networking event
	UFUNCTION(BlueprintCallable, Category = "SpatialOS", meta = (WorldContext = "WorldContextObject"))
	static bool ContinueLatencyTrace(UObject* WorldContextObject, const AActor* Actor, const FString& FunctionName, const FString& TraceDesc);

	// End a latency trace. This needs to be called within the receiving end of the traced networked event (ie. an rpc)
	UFUNCTION(BlueprintCallable, Category = "SpatialOS", meta = (WorldContext = "WorldContextObject"))
	static bool EndLatencyTrace(UObject* WorldContextObject);

	// Returns if we're in the receiving section of a network trace. If this is true, it's valid to continue or end it.
	UFUNCTION(BlueprintCallable, Category = "SpatialOS", meta = (WorldContext = "WorldContextObject"))
	static bool IsLatencyTraceActive(UObject* WorldContextObject);

	static const TraceKey InvalidTraceKey;

#if TRACE_LIB_ACTIVE

	// Internal GDK usage, shouldn't be used by game code
	static USpatialLatencyTracer* GetTracer(UObject* WorldContextObject);

	bool IsValidKey(TraceKey Key);
	TraceKey GetTraceKey(const UObject* Obj, const UFunction* Function);

	void MarkActiveLatencyTrace(const TraceKey Key);
	void WriteToLatencyTrace(const TraceKey Key, const FString& TraceDesc);
	void EndLatencyTrace(const TraceKey Key, const FString& TraceDesc);

	void WriteTraceToSchemaObject(const TraceKey Key, Schema_Object* Obj, const Schema_FieldId FieldId);
	TraceKey ReadTraceFromSchemaObject(Schema_Object* Obj, const Schema_FieldId FieldId);

	void SetWorkerId(const FString& NewWorkerId) { WorkerId = NewWorkerId; }
	void ResetWorkerId() { WorkerId = TEXT("Undefined"); }

	void OnEnqueueMessage(const SpatialGDK::FOutgoingMessage*);
	void OnDequeueMessage(const SpatialGDK::FOutgoingMessage*);

private:

	using ActorFuncKey = TPair<const AActor*, const UFunction*>;
	using TraceSpan = improbable::trace::Span;

	bool BeginLatencyTrace_Internal(const AActor* Actor, const FString& FunctionName, const FString& TraceDesc);
	bool ContinueLatencyTrace_Internal(const AActor* Actor, const FString& FunctionName, const FString& TraceDesc);
	bool EndLatencyTrace_Internal();
	bool IsLatencyTraceActive_Internal();

	TraceKey CreateNewTraceEntry(const AActor* Actor, const FString& FunctionName);
	TraceKey GenerateNewTraceKey();
	TraceSpan* GetActiveTrace();

	void WriteKeyFrameToTrace(const TraceSpan* Trace, const FString& TraceDesc);
	FString FormatMessage(const FString& Message) const;


	FString WorkerId;

	// This is used to track if there is an active trace within a currently processing network call. The user is
	// able to hook into this active trace, and `continue` it to another network relevant call. If so, the
	// ActiveTrace will be moved to another tracked trace.
	TraceKey ActiveTraceKey;
	TraceKey NextTraceKey = 1;

	FCriticalSection Mutex; // This mutex is to protect modifications to the containers below
	TMap<ActorFuncKey, TraceKey> TrackingTraces;
	TMap<TraceKey, TraceSpan> TraceMap;

public:

#endif // TRACE_LIB_ACTIVE

	// Used for testing trace functionality, will send a debug trace in three parts from this worker
	UFUNCTION(BlueprintCallable, Category = "SpatialOS")
	static void Debug_SendTestTrace();
};
