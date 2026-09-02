using Godot;

public partial class CSharpBindingSmoke : SceneTree
{
    public override void _Initialize()
    {
        try
        {
            CSharpFacadeTests.RunAll();
            GD.Print("Cesium C# binding smoke test passed");
            Quit(0);
        }
        catch (System.Exception exception)
        {
            GD.PushError($"Cesium C# binding smoke test failed: {exception}");
            Quit(1);
        }
    }

}
