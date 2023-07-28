# Graph Formatter for Unreal Engine

Graph Formatter is an Unreal Engine plugin that can arrange graph nodes automatically.

![Usage](https://user-images.githubusercontent.com/49013103/209635898-3d7c4b43-fdf7-408c-817b-11b80a980862.gif)

## Installing
###### Install From Source

Clone or [download](https://github.com/howaajin/graphformatter/releases) this repository to the "[root of your project]/Plugins/GraphFormatter" and restart Editor.  
Note: git head version only support latest engine version. If you download the source code, you will need a compilation environment. Here I provide the compiled Win64 version.

###### Install From Marketplace 
https://www.unrealengine.com/marketplace/graph-formatter

## Usage

Select nodes you want to arrange, or just deselect all nodes and press "Format Graph" button on the toolbar.  
Configure it in "Editor Preferences/Plugins/Graph Formatter".  

Regarding the [PCG Graph](https://docs.unrealengine.com/5.2/en-US/procedural-content-generation-overview/), this plugin is now functional with keyboard shortcuts. However, due to the difference in how the PCG Graph extends the Toolbar compared to other Graph editors, it causes subsequent extensions to disappear.

[Enable it in more editors](https://github.com/howaajin/graphformatter/wiki/Enable-it-in-more-editors)  
[Parameter grouping](https://github.com/howaajin/graphformatter/wiki/With-and-without-parameter-grouping)  
[Enable by default for all project](https://github.com/howaajin/graphformatter/wiki/Make-a-plugin-enabled-by-default-for-all-projects)  
[The useful Place Block command](https://github.com/howaajin/graphformatter/wiki/Usage-of-PlaceBlock-command)  
You can find more details in the [Wiki](https://github.com/howaajin/graphformatter/wiki).

## Technical Details

It is based on the ideas of [Layered graph drawing](https://en.wikipedia.org/wiki/Layered_graph_drawing).  
References:
[Fast and Simple Horizontal Coordinate Assignment](https://link.springer.com/chapter/10.1007/3-540-45848-4_3).  
[Size- and Port-Aware Horizontal Node Coordinate Assignment](https://link.springer.com/chapter/10.1007/978-3-319-27261-0_12).  

## Purchase my other works to support me

[(New)Bring back the system shadow to the Unreal Editor in Windows](https://www.unrealengine.com/marketplace/product/editor-windows-frame)

[Marketplace page](https://www.unrealengine.com/marketplace/zh-CN/profile/FeiSu?count=20&sortBy=effectiveDate&sortDir=DESC&start=0)
