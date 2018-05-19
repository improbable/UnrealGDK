using System.Collections.Generic;
using Improbable.CodeGeneration.FileHandling;
using Improbable.CodeGeneration.Jobs;
using Improbable.Unreal.CodeGeneration.SchemaProcessing.Details;

namespace Improbable.Unreal.CodeGeneration.Jobs
{
    public class UnrealComponentJob : CodegenJob
    {
        public UnrealComponentJob(UnrealComponentDetails unrealComponent, string outputDirectory, IFileSystem fileSystem)
            : base(outputDirectory, fileSystem)
        {
            InputFiles = new List<string>() { unrealComponent.UnderlyingPackageDetails.SourceSchema };

            var componentHeaderFileName = unrealComponent.CapitalisedName + componentHeaderSuffix;
            var componentImplFileName = unrealComponent.CapitalisedName + componentImplSuffix;
            var componentUpdateHeaderFileName = unrealComponent.CapitalisedName + componentUpdatedHeaderSuffix;
            var componentUpdateImplFileName = unrealComponent.CapitalisedName + componentUpdateImplSuffix;
            var opWrapperHeaderFileName = unrealComponent.CapitalisedName + addComponentHeaderSuffix;

            OutputFiles = new List<string>()
            {
                componentHeaderFileName,
                componentImplFileName,
                componentUpdateHeaderFileName,
                componentUpdateImplFileName,
                opWrapperHeaderFileName
            };

            this.unrealComponent = unrealComponent;
        }

        protected override void RunImpl()
        {
            var headerGenerator = new UnrealComponentHeaderGenerator(unrealComponent);
            var componentHeaderFileName = unrealComponent.CapitalisedName + componentHeaderSuffix;
            Content.Add(componentHeaderFileName, headerGenerator.TransformText());

            var implementationGenerator = new UnrealComponentImplementationGenerator(unrealComponent);
            var componentImplFileName = unrealComponent.CapitalisedName + componentImplSuffix;
            Content.Add(componentImplFileName, implementationGenerator.TransformText());

            var updateHeaderGenerator = new UnrealComponentUpdateHeaderGenerator(unrealComponent);
            var componentUpdateHeaderFileName = unrealComponent.CapitalisedName + componentUpdatedHeaderSuffix;
            Content.Add(componentUpdateHeaderFileName, updateHeaderGenerator.TransformText());

            var updateImplementationGenerator = new UnrealComponentUpdateImplementationGenerator(unrealComponent);
            var componentUpdateImplFileName = unrealComponent.CapitalisedName + componentUpdateImplSuffix;
            Content.Add(componentUpdateImplFileName, updateImplementationGenerator.TransformText());
            
            var opWrapperGenerator = new UnrealAddEntityOpWrapperHeaderGenerator(unrealComponent);
            var opWrapperHeaderFileName = unrealComponent.CapitalisedName + addComponentHeaderSuffix;
            Content.Add(opWrapperHeaderFileName, opWrapperGenerator.TransformText());
        }

        private const string componentHeaderSuffix = "Component.h";
        private const string componentImplSuffix = "Component.cpp";
        private const string componentUpdatedHeaderSuffix = "ComponentUpdate.h";
        private const string componentUpdateImplSuffix = "ComponentUpdate.cpp";
        private const string addComponentHeaderSuffix = "AddComponentOp.h";

        private readonly UnrealComponentDetails unrealComponent;
    }
}
