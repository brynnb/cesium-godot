using System.Reflection;
using System.Runtime.ExceptionServices;
using CesiumForGodot;
using twodog.Testing;
using twodog.Testing.Xunit;

namespace CesiumCSharpTwoDog.Tests;

[Collection<HeadlessCollection>]
public class CesiumStreamingCompatibilityTests(HeadlessFixture godot)
{
    // Deliberately synchronous and without xUnit's Timeout property. Godot
    // operations must stay on the thread that created the LibGodot instance.
    [Fact]
    public void ExistingStreamingContractPassesUnderLibGodot()
    {
        Assembly assembly = typeof(Cesium3DTileset).Assembly;
        Type streamingTests = assembly.GetType(
            "CSharpStreamingIntegrationTests",
            throwOnError: true
        )!;
        MethodInfo runAsync = streamingTests.GetMethod(
            "RunAsync",
            BindingFlags.Public | BindingFlags.Static
        )!;
        var testTask = (Task)runAsync.Invoke(null, [godot.Tree])!;

        // LibGodot does not run frames in the background. ProcessFrame awaits
        // cannot complete unless the embedding host explicitly iterates Godot.
        for (int frame = 0; frame < 2_000 && !testTask.IsCompleted; frame++)
        {
            godot.GodotInstance.Iteration();
            Thread.Sleep(1);
        }

        Assert.True(testTask.IsCompleted, "streaming test exceeded the LibGodot frame budget");
        if (testTask.IsFaulted)
            ExceptionDispatchInfo.Capture(testTask.Exception!.GetBaseException()).Throw();
        Assert.Equal(TaskStatus.RanToCompletion, testTask.Status);
    }
}
