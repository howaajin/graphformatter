# Graph Formatter for Unreal Engine

Graph Formatter is an Unreal Engine plugin that can arrange graph nodes automatically.

![Usage](https://user-images.githubusercontent.com/49013103/209635898-3d7c4b43-fdf7-408c-817b-11b80a980862.gif)

## Installing
###### Install From Source

Clone or [download](https://github.com/howaajin/graphformatter/releases) this repository to the "[root of your project]/Plugins/GraphFormatter" and restart Editor.  
Note: git head version only support latest engine version. If you download the source code, you will need a compilation environment (Visual Studio on Windows). Here I provide the compiled Win64 version.

###### Install From Marketplace 
https://www.unrealengine.com/marketplace/graph-formatter

## Usage

Select nodes you want to arrange, or just deselect all nodes and press "Format Graph" button on the toolbar.  
Configure it in "Editor Preferences/Plugins/Graph Formatter".  

Regarding the [PCG Graph](https://docs.unrealengine.com/5.2/en-US/procedural-content-generation-overview/), this plugin is now functional with keyboard shortcuts. However, due to the difference in how the PCG Graph extends the Toolbar compared to other Graph editors, it causes subsequent extensions to disappear.

Please note that this plugin uses template specialization to access private member pointers, so the compiled versions must match exactly. For example, a Debug version cannot connect to an engine version built for Development.

[Enable it in more editors](https://github.com/howaajin/graphformatter/wiki/Enable-it-in-more-editors)  
[Parameter grouping](https://github.com/howaajin/graphformatter/wiki/With-and-without-parameter-grouping)  
[Enable by default for all project](https://github.com/howaajin/graphformatter/wiki/Make-a-plugin-enabled-by-default-for-all-projects)  
[The useful Place Block command](https://github.com/howaajin/graphformatter/wiki/Usage-of-PlaceBlock-command)  
You can find more details in the [Wiki](https://github.com/howaajin/graphformatter/wiki).

## Formatting Blueprint graphs through MCP (UE 5.8)

AI tools can create working Blueprint graphs but often leave overlapping nodes,
long wires, and layouts that are difficult to read. The MCP tool lets an AI
client finish a graph edit by running Graph Formatter's existing layout
algorithm, using your spacing and positioning settings.

This branch requires UE 5.8's built-in experimental **Toolset Registry** plugin,
which Graph Formatter enables as a dependency. To expose its tool over MCP,
also enable the built-in **Unreal MCP** plugin (`ModelContextProtocol`) in your
project and restart the editor. Start its server using the engine's MCP settings
or the `-ModelContextProtocolStartServer` command-line option, then connect your
MCP client to the configured endpoint (normally `http://localhost:8000/mcp`).
Graph Formatter does not start the server itself.

In the default tool-search mode, use `list_toolsets` to find
`GraphFormatter.GraphFormatterToolset`, then `describe_toolset` to read its
schema. Invoke `call_tool` with:

```json
{
  "toolset_name": "GraphFormatter.GraphFormatterToolset",
  "tool_name": "FormatBlueprintGraph",
  "arguments": {
    "graph": {
      "refPath": "/Game/Blueprints/BP_Example.BP_Example:EventGraph"
    }
  }
}
```

Use the actual object path of the graph you want to arrange. The tool formats
all nodes and comments in that one graph, including nodes outside the viewport.
It measures an offscreen graph widget, so no asset editor needs to be open and
the operation does not depend on the mouse position or take desktop focus.
The toolbar and keyboard commands keep their existing behavior.

Read `returnValue.bSuccess` and `returnValue.error` before continuing. The result
also reports `nodeCount`, `movedNodeCount`, `resizedCommentCount`, and
`bPackageDirty`. Formatting changes layout in an undo transaction; it does not
compile, save, or check out the Blueprint. Review the layout and validate the
Blueprint before saving it through your normal workflow.

The tool accepts editable K2 graphs owned by initialized Blueprints. It refuses
PIE or simulation, unsupported graphs, active Blueprint compilation, and invalid
node ownership or GUIDs. Material, Niagara, behavior tree, and other non-K2
graphs are outside this tool's scope. An idle editor with Slate initialized is
required; this is not a standalone commandlet formatter.

### Automation tests

In an idle editor, run the `GraphFormatter.MCP` tests from Session Frontend, or
run them in a separate editor process:

```text
UnrealEditor-Cmd.exe "YourProject.uproject" /Engine/Maps/Entry -unattended -nopause -nosplash -nullrhi -nosound -ExecCmds="Automation RunTests GraphFormatter.MCP" -TestExit="Automation Test Queue Empty" -ReportOutputPath="C:/Temp/GraphFormatterTests"
```

`Validation` checks toolset registration, rejected inputs, and PIE state.
`LayoutAndUndo` checks linked
node placement, comment resizing, preserved node identities and connections,
and undo restoring the original positions and comment bounds. The tests create
temporary Blueprint fixtures and do not save assets.

## Useful Tips

1. **Comment-Based Grouping:** This plugin employs comment nodes to form groups. Grouping complex nodes based on comments can lead to more effective outcomes.

2. **Managing Node Complexity:** In situations where a node has an excessive number of incoming or outgoing connections, both manual and automatic arrangement might not result in a tidy structure. In such cases, it's advisable make the result as a variable and duplicate new nodes for each usage of this variable.

3. **Avoiding Lengthy Connections:** Instead, consider utilizing variables or Reroute nodes within the material editor to maintain clarity and organization.

## Technical Details

It is based on the ideas of [Layered graph drawing](https://en.wikipedia.org/wiki/Layered_graph_drawing).  
References:
[Fast and Simple Horizontal Coordinate Assignment](https://link.springer.com/chapter/10.1007/3-540-45848-4_3).  
[Size- and Port-Aware Horizontal Node Coordinate Assignment](https://link.springer.com/chapter/10.1007/978-3-319-27261-0_12).  

## Purchase my other works to support me

[(New)Bring back the system shadow to the Unreal Editor in Windows](https://www.unrealengine.com/marketplace/product/editor-windows-frame)

[Marketplace page](https://www.unrealengine.com/marketplace/zh-CN/profile/FeiSu?count=20&sortBy=effectiveDate&sortDir=DESC&start=0)
