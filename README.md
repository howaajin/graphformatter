# Graph Formatter for Unreal Engine

Graph Formatter is an Unreal Engine plugin that can arrange graph nodes automatically.

![Usage](https://user-images.githubusercontent.com/49013103/209635898-3d7c4b43-fdf7-408c-817b-11b80a980862.gif)

## Installing
###### Install From Source

Clone or [download](https://github.com/howaajin/graphformatter/releases) this repository to the "[root of your project]/Plugins/GraphFormatter" and restart Editor.  
Note: git head version only support latest engine version.

###### Install From Marketplace 
https://www.unrealengine.com/marketplace/graph-formatter

## Usage

Select nodes you want to arrange, or just deselect all nodes and press "Format Graph" button on the toolbar.  
Configure it in "Editor Preferences/Plugins/Graph Formatter".  
[Enable it in more editors](https://github.com/howaajin/graphformatter/wiki/Enable-it-in-more-editors)  
You can find more details in the [Wiki](https://github.com/howaajin/graphformatter/wiki).

## Technical Details

It is based on the ideas of [Layered graph drawing](https://en.wikipedia.org/wiki/Layered_graph_drawing).  
References:
[Fast and Simple Horizontal Coordinate Assignment](https://link.springer.com/chapter/10.1007/3-540-45848-4_3).  
[Size- and Port-Aware Horizontal Node Coordinate Assignment](https://link.springer.com/chapter/10.1007/978-3-319-27261-0_12).  
