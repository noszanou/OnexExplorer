# OnexExplorer #

OnexExplorer is an open-source tool for unpacking and repacking .NOS data files from the game NosTale. It opens almost every .NOS archive, shows the content, exports and replaces it, and lets you add your own. Beyond the archive browser it carries dedicated editors: maps, VFX, 3D models, textures, sprites and the game's own .dat tables. It runs on Qt 6.11+ with OpenGL.

**If you paid for this software, you got scammed. It is free.**

## What it does ##

### Explorer ###

The archive tree. Open any .NOS, search it, preview an entry, export it, replace it, import custom content, and save the archive back. Files can be dropped onto the window. Each entry keeps its own record tag and compression flag, so an archive that is not edited saves back the way it came.

### Map Editor ###

Opens NStuData.NOS and the NStcData.NOS collision grids next to it.

- A searchable map list, because the archive holds several hundred maps.
- A textured 3D view of the whole map, with fly controls, object picking, dragging and rotating.
- Objects are drawn with the material their record carries: the tint, and on a type 3 node the blend factors and the billboard byte. A glow decal reads as a soft light rather than as a solid disc.
- Props whose mesh carries keyframes animate, each with its own phase offset from the record.
- Add, copy and remove objects; edit the model table, the ambient, object and map colours, the fog and the camera rotation.
- The collision grid is editable side by side with the 3D view, and placing an object blocks the cells under it.

### VFX Viewer and Creator ###

One tab, one archive, two modes. Opens NSeffData.NOS.

**Browse** lists every effect with a search field, decodes it to editable JSON, and plays it in a 3D view with a play/pause button and a scrubbable timeline. Entries can be duplicated and deleted. An effect can be sent to the Create panel to be reworked and saved under a new id.

**Create** builds an effect layer by layer, with every field of an object and all eight keyframe tracks, and a live preview that follows what you type. A layer can be copied and pasted into another effect.

The 3D view reproduces what the client does rather than approximating it:

- The client's own camera: a 12 degree lens at tilt 45, yaw -45, thirty units out, with the 0.6 lift along the camera's own up axis.
- The full fourteen-entry blend table, including the three legacy enums 41 stock emitters carry, and its ZERO default for anything unlisted.
- The billboard case set of the `+0x05` byte, the per-emitter depth-write byte, the per-batch cull byte, the alpha test, and the animated UV matrix that makes a beam scroll and an aura spin.
- The mesh node forest composed child onto parent, which is what puts a multi-part mesh together.
- The emitter rules: the detail gate, the undefined clock, the lifetime window, the ground channel, and the authored "nothing here" geometry id that 3347 of the 43021 stock emitters carry. An emitter the client skips is skipped here, counted and named under the view instead of being drawn as a square.
- The KIND-2 particle system: the client's own Delphi LCG drawn in its own order, its steps-quantised spawn lattice, its closed-form motion and its 500-slot pool. Shower, bounds or off, with a reseed button.

Still a reconstruction in three places, all stated in the tool: the RNG seed is process-global in the client, the frame length varies, and the particle pool is shared with every other effect on screen.

### Model Viewer ###

For NStgData and NStgeData meshes.

- The client's camera by default, unlockable to a free orbit.
- Parts list, per-part highlight, wireframe, colour per part.
- The animation the mesh carries, with a timeline and a keyframe editor.
- Textures loaded from the volumes, or from a file, or picked from the volumes.
- Retexture the selected part alone instead of every batch of the model.
- Paint straight onto the model: a click finds the triangle under the cursor, interpolates the texture coordinate and paints the page there. Painted pages are written back on demand.
- Export and import .obj with the node placements baked in, so a model opens assembled in a modelling tool and comes back the way it left.

### Texture Editor ###

Reachable from the VFX texture track, the texture picker and the model viewer.

- Hue, saturation, brightness and opacity.
- A brush with colour, size and eraser; undo and redo.
- A before and after view.
- Replace the page from an image file, or export it as PNG.
- Save over the same id or out to a new one. The page keeps its own size, its own pixel format and its mip chain, and is written into the volume that holds it, with a .bak kept.
- Batch export and import: every page a VFX texture track names goes out to a folder as `<id>.png` and comes back from one.

### Used by ###

Before overwriting a page or a mesh, ask what reads it. One pass over NSeffData and NStuData answers which effects and which maps name it.

### Dat Editor ###

NSgtdData records with a search, the language archives next to it, the icon archive, and a BCard picker so a card is chosen by its wording rather than by its number.

### Animations, Audio, Splash, Video, Import ###

Sprite and animation archives with playback, sound packs and lookup tables, the splash pictures embedded in the .ews executables, video entries, and a custom-content merge.

### Game folder ###

Set the NostaleData folder once in the settings and the tabs stop asking: the VFX viewer, the map editor and the dat editor open their own archive, and the 3D views resolve models and textures from it.


## Currently supported archives ##

- NSgtdData (Items, quests, skills, mobs etc. data)
- NSlangData (_code_lang_list (ztsXXXe))
- NScliData (constring.dat)
- NSetcData (Word list for Memory & TabooStr.lst)
- NS4BbData (Big images)
- NSipData (Icons)
- NStpData (Map and model textures)
- NStpeData (Effect textures)
- NStpuData (UI textures)
- NStcData (Map Grids)
- NSmpData (Monster-related sprites)
- NSppData (Player-related sprites)
- NSmnData (Mob-related sprite infos)
- NSpnData (Player-related sprite infos)
- NSmcData (Monster-related animation kits)
- NSpcData (Player-related animation kits)
- NStgData (3D Models)
- NStgeData (3D VFX models)
- NStuData (Map configs)
- NSpmData (Sprite part draw order / part z-order)
- NStsData (Placeholders for something ??)
- NStkData (Placeholders - map related)
- NSeffData (VFX configs)
- NSemData (VFX Motion)
- NSesData (VFX Timeline)
- NSedData (VFX Color)
- NSgrdData (Ground mesh)


## Notes on the format ##

Two conventions are per family and are not free choices. The tool follows what the archive it opened already does rather than a default.

- **Compression.** NSip, NSmp, NSpp, NStc and NStu store every entry zlib-compressed; every other family stores them raw. NSip, NSmp and NSpp are written at level 1, NStc, NStu and NS4Bb at level 9.
- **The record tag is not a timestamp.** It is packed decimal written in hex, so 0x20050130 reads as 2005-01-30, and each family carries one fixed value. The texture families are the exception and carry two, mixed inside one volume, so a new page takes the one its volume mostly uses.

An archive whose directory points past the end of the file is incomplete, usually an interrupted copy. The tool names the entries it had to leave out instead of failing on them.


## License ##

Our project is based on **Boost Software License**:
> [LICENSE](/LICENSE)


## Installation ##

**Download** a **[release](https://github.com/noszanou/OnexExplorer/releases)** or:

1. Compile the source with Qt 6.11+ and OpenGL (freeglut)
2. Move the required Qt DLLs next to the EXE
3. Run the program

The experimental branch and releases flagged **pre-release** are in development and do not guarantee that everything works as expected.
**If you have any problems please try to compile the master branch or use regular releases!**


## Report bugs and suggest features ##

If you have any **problems** with OnexExplorer or want to suggest new **features** feel free to open **[issues](https://github.com/Pumbaa98/OnexExplorer/issues)**.
Please don't forget to mention the **version** you're using.


## Credits ##

- OnexTeam, for the original tool.
- Pumbaa, for maintaining it.
