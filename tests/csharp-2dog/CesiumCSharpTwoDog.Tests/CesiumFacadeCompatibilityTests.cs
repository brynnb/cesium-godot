using System.Reflection;
using CesiumForGodot;
using twodog.Testing;
using twodog.Testing.Xunit;

namespace CesiumCSharpTwoDog.Tests;

[Collection<HeadlessCollection>]
public class CesiumFacadeCompatibilityTests(HeadlessFixture godot)
{
    [Fact]
    public void ExistingFacadeContractPassesUnderLibGodot()
    {
        Assembly assembly = typeof(Cesium3DTileset).Assembly;
        Type facadeTests = assembly.GetType("CSharpFacadeTests", throwOnError: true)!;
        MethodInfo runAll = facadeTests.GetMethod(
            "RunAll",
            BindingFlags.Public | BindingFlags.Static
        )!;

        Assert.NotNull(godot.Tree);
        runAll.Invoke(null, null);
    }
}
