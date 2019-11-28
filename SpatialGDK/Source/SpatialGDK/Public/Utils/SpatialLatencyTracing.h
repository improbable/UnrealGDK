// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"

#include "SpatialConstants.h"
#include "Map.h"

#if TRACE_LIB_ACTIVE
#include "WorkerSDK/improbable/trace.h"
#endif

#include "SpatialLatencyTracing.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpatialLatencyTracing, Log, All);

class AActor;
class UFunction;
class USpatialGameInstance;

using TraceKey = int32;

UCLASS()
class SPATIALGDK_API USpatialLatencyTracing : public UObject
{
	GENERATED_BODY()

public:

	// Front-end exposed, allows users to register, start, continue, and end traces
	UFUNCTION(BlueprintCallable, Category = "SpatialOS", meta = (WorldContext = "WorldContextObject"))
	static void RegisterProject(UObject* WorldContextObject, const FString& ProjectId);

	UFUNCTION(BlueprintCallable, Category = "SpatialOS", meta = (WorldContext = "WorldContextObject"))
	static bool BeginLatencyTrace(UObject* WorldContextObject, const AActor* Actor, const FString& FunctionName, const FString& TraceDesc);

	UFUNCTION(BlueprintCallable, Category = "SpatialOS", meta = (WorldContext = "WorldContextObject"))
	static bool ContinueLatencyTrace(UObject* WorldContextObject, const AActor* Actor, const FString& FunctionName, const FString& TraceDesc);

	UFUNCTION(BlueprintCallable, Category = "SpatialOS", meta = (WorldContext = "WorldContextObject"))
	static bool EndLatencyTrace(UObject* WorldContextObject);

	static const TraceKey ActiveTraceKey = 0;
	static const TraceKey InvalidTraceKey = -1;

#if TRACE_LIB_ACTIVE

	// Internal GDK usage, shouldn't be used by game code
	static USpatialLatencyTracing* GetTracer(UObject* WorldContextObject);

	bool IsValidKey(const TraceKey& Key);
	TraceKey GetTraceKey(const UObject* Obj, const UFunction* Function);

	void WriteToLatencyTrace(const TraceKey& Key, const FString& TraceDesc);
	void EndLatencyTrace(const TraceKey& Key, const FString& TraceDesc);

	void WriteTraceToSchemaObject(const TraceKey& Key, Schema_Object* Obj);
	TraceKey ReadTraceFromSchemaObject(Schema_Object* Obj);

private:

	using ActorFuncKey = TPair<const AActor*, const UFunction*>;
	using TraceSpan = improbable::trace::Span;

	bool BeginLatencyTrace_Internal(const AActor* Actor, const FString& FunctionName, const FString& TraceDesc);
	bool ContinueLatencyTrace_Internal(const AActor* Actor, const FString& FunctionName, const FString& TraceDesc);
	bool EndLatencyTrace_Internal();

	TraceKey CreateNewTraceEntry(const AActor* Actor, const FString& FunctionName);
	TraceSpan* GetActiveTrace();

	void WriteKeyFrameToTrace(const TraceSpan* Trace, const FString& TraceDesc);

	TMap<ActorFuncKey, TraceKey> TrackingTraces;
	TMap<TraceKey, TraceSpan> TraceMap;

	FCriticalSection Mutex;

public:

#endif // TRACE_LIB_ACTIVE

	UFUNCTION(BlueprintCallable, Category = "SpatialOS")
	static void SendTestTrace();
};
