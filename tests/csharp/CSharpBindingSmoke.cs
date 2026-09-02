using Godot;
using System.Threading.Tasks;

public partial class CSharpBindingSmoke : SceneTree
{
    public override void _Initialize()
    {
        _ = RunAsync();
    }

    private async Task RunAsync()
    {
        try
        {
            CSharpFacadeTests.RunAll();
            await CSharpStreamingIntegrationTests.RunAsync(this);
            GD.Print("Cesium C# facade and local streaming integration tests passed");
            Quit(0);
        }
        catch (System.Exception exception)
        {
            GD.PushError($"Cesium C# integration test failed: {exception}");
            Quit(1);
        }
    }
}
