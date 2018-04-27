﻿//------------------------------------------------------------------------------
// <auto-generated>
//     This code was generated by a tool.
//     Runtime Version:4.0.30319.42000
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
// </auto-generated>
//------------------------------------------------------------------------------

namespace Improbable.Unreal.CodeGeneration {
    using System.Collections.Generic;
    using System;
    using Improbable.CodeGeneration.Jobs;
    
    
    public partial class UnrealEntityPipelineHeaderGenerator : UnrealEntityPipelineHeaderGeneratorBase {
        
        public virtual string TransformText() {
            this.GenerationEnvironment = null;
            
            #line 3 "Templates\UnrealEntityPipelineHeaderGenerator.tt"



            
            #line default
            #line hidden
            
            #line 6 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
            this.Write(@"// Copyright (c) Improbable Worlds Ltd, All Rights Reserved
// ===========
// DO NOT EDIT - this file is automatically regenerated.
// =========== 

#pragma once

#include ""CoreMinimal.h""
#include ""SpatialOSViewTypes.h""
#include ""SpatialOSWorkerTypes.h""
#include ""EntityPipelineBlock.h""
#include ""ScopedViewCallbacks.h""
");
            
            #line default
            #line hidden
            
            #line 18 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
 foreach (var unrealComponent in unrealComponents) { 
            
            #line default
            #line hidden
            
            #line 19 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
            this.Write("#include \"");
            
            #line default
            #line hidden
            
            #line 19 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
            this.Write(this.ToStringHelper.ToStringWithCulture( unrealComponent.CapitalisedName ));
            
            #line default
            #line hidden
            
            #line 19 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
            this.Write("AddComponentOp.h\"\r\n");
            
            #line default
            #line hidden
            
            #line 20 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
 } 
            
            #line default
            #line hidden
            
            #line 21 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
            this.Write(@"#include ""EntityPipeline.generated.h""

UCLASS()
class SPATIALOS_API UEntityPipeline : public UObject
{
	GENERATED_BODY()

public:
	UEntityPipeline();

	/**
	* Initialise the UEntityPipeline. Calling Init() more than once results in an error.
	*/
	void Init(const TWeakPtr<SpatialOSView>& InView, UCallbackDispatcher* InCallbackDispatcher);

	/**
	* Deregister all callbacks. Init() may be called again after this method is called.
	*/
	void DeregisterAllCallbacks();

	void AddBlock(UEntityPipelineBlock* NewBlock);
	void ProcessOps(const TWeakPtr<SpatialOSView>& InView, const TWeakPtr<SpatialOSConnection>& InConnection, UWorld* World);

	void OnAddEntity(const worker::AddEntityOp& Op) { FirstBlock->AddEntity(Op); }
	void OnRemoveEntity(const worker::RemoveEntityOp& Op) { FirstBlock->RemoveEntity(Op); }
	void OnRemoveComponent(const worker::ComponentId ComponentId, const worker::RemoveComponentOp& Op) { FirstBlock->RemoveComponent(ComponentId, Op); }
	void OnAuthorityChange(const worker::ComponentId ComponentId, const worker::AuthorityChangeOp& Op) { FirstBlock->ChangeAuthority(ComponentId, Op); }

");
            
            #line default
            #line hidden
            
            #line 49 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
 foreach (var unrealComponent in unrealComponents) { 
            
            #line default
            #line hidden
            
            #line 50 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
            this.Write("\tvoid Add");
            
            #line default
            #line hidden
            
            #line 50 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
            this.Write(this.ToStringHelper.ToStringWithCulture( unrealComponent.CapitalisedName ));
            
            #line default
            #line hidden
            
            #line 50 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
            this.Write("ComponentOp(const worker::AddComponentOp<");
            
            #line default
            #line hidden
            
            #line 50 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
            this.Write(this.ToStringHelper.ToStringWithCulture( unrealComponent.UnderlyingQualifiedName ));
            
            #line default
            #line hidden
            
            #line 50 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
            this.Write(">& Op);\r\n\tvoid Remove");
            
            #line default
            #line hidden
            
            #line 51 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
            this.Write(this.ToStringHelper.ToStringWithCulture( unrealComponent.CapitalisedName ));
            
            #line default
            #line hidden
            
            #line 51 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
            this.Write("ComponentOp(const worker::RemoveComponentOp& Op);\r\n");
            
            #line default
            #line hidden
            
            #line 52 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
 } 
            
            #line default
            #line hidden
            
            #line 53 "Templates\UnrealEntityPipelineHeaderGenerator.tt"
            this.Write(@"
private:
	UPROPERTY()
	UEntityPipelineBlock* FirstBlock;
	UPROPERTY()
	UEntityPipelineBlock* LastBlock;
	UPROPERTY()
	UCallbackDispatcher* CallbackDispatcher;

	bool bInitialised;
	improbable::unreal::callbacks::FScopedViewCallbacks Callbacks;
};
");
            
            #line default
            #line hidden
            return this.GenerationEnvironment.ToString();
        }
        
        public virtual void Initialize() {
        }
    }
    
    public class UnrealEntityPipelineHeaderGeneratorBase {
        
        private global::System.Text.StringBuilder builder;
        
        private global::System.Collections.Generic.IDictionary<string, object> session;
        
        private global::System.CodeDom.Compiler.CompilerErrorCollection errors;
        
        private string currentIndent = string.Empty;
        
        private global::System.Collections.Generic.Stack<int> indents;
        
        private ToStringInstanceHelper _toStringHelper = new ToStringInstanceHelper();
        
        public virtual global::System.Collections.Generic.IDictionary<string, object> Session {
            get {
                return this.session;
            }
            set {
                this.session = value;
            }
        }
        
        public global::System.Text.StringBuilder GenerationEnvironment {
            get {
                if ((this.builder == null)) {
                    this.builder = new global::System.Text.StringBuilder();
                }
                return this.builder;
            }
            set {
                this.builder = value;
            }
        }
        
        protected global::System.CodeDom.Compiler.CompilerErrorCollection Errors {
            get {
                if ((this.errors == null)) {
                    this.errors = new global::System.CodeDom.Compiler.CompilerErrorCollection();
                }
                return this.errors;
            }
        }
        
        public string CurrentIndent {
            get {
                return this.currentIndent;
            }
        }
        
        private global::System.Collections.Generic.Stack<int> Indents {
            get {
                if ((this.indents == null)) {
                    this.indents = new global::System.Collections.Generic.Stack<int>();
                }
                return this.indents;
            }
        }
        
        public ToStringInstanceHelper ToStringHelper {
            get {
                return this._toStringHelper;
            }
        }
        
        public void Error(string message) {
            this.Errors.Add(new global::System.CodeDom.Compiler.CompilerError(null, -1, -1, null, message));
        }
        
        public void Warning(string message) {
            global::System.CodeDom.Compiler.CompilerError val = new global::System.CodeDom.Compiler.CompilerError(null, -1, -1, null, message);
            val.IsWarning = true;
            this.Errors.Add(val);
        }
        
        public string PopIndent() {
            if ((this.Indents.Count == 0)) {
                return string.Empty;
            }
            int lastPos = (this.currentIndent.Length - this.Indents.Pop());
            string last = this.currentIndent.Substring(lastPos);
            this.currentIndent = this.currentIndent.Substring(0, lastPos);
            return last;
        }
        
        public void PushIndent(string indent) {
            this.Indents.Push(indent.Length);
            this.currentIndent = (this.currentIndent + indent);
        }
        
        public void ClearIndent() {
            this.currentIndent = string.Empty;
            this.Indents.Clear();
        }
        
        public void Write(string textToAppend) {
            this.GenerationEnvironment.Append(textToAppend);
        }
        
        public void Write(string format, params object[] args) {
            this.GenerationEnvironment.AppendFormat(format, args);
        }
        
        public void WriteLine(string textToAppend) {
            this.GenerationEnvironment.Append(this.currentIndent);
            this.GenerationEnvironment.AppendLine(textToAppend);
        }
        
        public void WriteLine(string format, params object[] args) {
            this.GenerationEnvironment.Append(this.currentIndent);
            this.GenerationEnvironment.AppendFormat(format, args);
            this.GenerationEnvironment.AppendLine();
        }
        
        public class ToStringInstanceHelper {
            
            private global::System.IFormatProvider formatProvider = global::System.Globalization.CultureInfo.InvariantCulture;
            
            public global::System.IFormatProvider FormatProvider {
                get {
                    return this.formatProvider;
                }
                set {
                    if ((value != null)) {
                        this.formatProvider = value;
                    }
                }
            }
            
            public string ToStringWithCulture(object objectToConvert) {
                if ((objectToConvert == null)) {
                    throw new global::System.ArgumentNullException("objectToConvert");
                }
                global::System.Type type = objectToConvert.GetType();
                global::System.Type iConvertibleType = typeof(global::System.IConvertible);
                if (iConvertibleType.IsAssignableFrom(type)) {
                    return ((global::System.IConvertible)(objectToConvert)).ToString(this.formatProvider);
                }
                global::System.Reflection.MethodInfo methInfo = type.GetMethod("ToString", new global::System.Type[] {
                            iConvertibleType});
                if ((methInfo != null)) {
                    return ((string)(methInfo.Invoke(objectToConvert, new object[] {
                                this.formatProvider})));
                }
                return objectToConvert.ToString();
            }
        }
    }
}
