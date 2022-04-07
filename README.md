# Graph Formatter for Unreal Engine

Graph Formatter is an Unreal Engine plugin that can arrange graph nodes automatically.

#### Installing
##### Install From Source

Note: git head version only support latest engine version.
Clone or download this repository to the "[root of your project]/Plugins/GraphFormatter" and restart Editor.

##### Install From Marketplace 
https://www.unrealengine.com/marketplace/graph-formatter

#### Usage

Select nodes you want to arrange, or just deselect all nodes and press "Format Graph" button on the toolbar.

Configure it in "Editor Preferences/Plugins/Graph Formatter".

#### Technical Details

It is based on the ideas of [Layered graph drawing](https://en.wikipedia.org/wiki/Layered_graph_drawing).

References:

[Fast and Simple Horizontal Coordinate Assignment](https://link.springer.com/chapter/10.1007/3-540-45848-4_3).

[Size- and Port-Aware Horizontal Node Coordinate Assignment](https://link.springer.com/chapter/10.1007/978-3-319-27261-0_12).
