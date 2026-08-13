# MikkTSpace

This directory contains the unmodified upstream MikkTSpace 1.0 implementation
from <https://github.com/mmikk/MikkTSpace> at commit
`3e895b49d05ea07e4c2133156cfa94369e19e409`.

Cesium for Unreal v2.29.0 uses MikkTSpace when a normal-mapped glTF primitive
does not provide authored tangents. Cesium for Godot vendors the same reference
algorithm so generated tangent frames agree with normal-map baking tools and
the upstream renderer. The license notice is preserved at the top of both
source files.
