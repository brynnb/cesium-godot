# Generated C# facade

The `.cs` files in this directory are generated from the real loaded
GDExtension API. Do not edit them by hand. Maintainers regenerate them with:

```bash
python3 tools/generate_csharp_bindings.py
```

The generator is fetched from the exact fork revision recorded in
`dependencies/csharp-bindgen.lock.json`; addon users do not need the generator.
