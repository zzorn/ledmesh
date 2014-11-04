Ledmesh
=======

Overview
--------

Mesh networked nodes controlling LED light strips, LED spotlights, sensors, and other things.

The network uses wireless radio modules to communicate between nodes.

Ledmesh can be used to create coherent led lighting effects in a space (e.g. light waves flowing around walls), 
as well as for controlling 12V lighting (e.g. led spotlights).

Ledmesh can also be used as an information radiator, connecting some nodes up to computers or suitable sensors, 
and sending out light waves to nearby nodes or doing light effect changes for the whole network on different events.

Ledmesh needs configuration after installation, to tell nodes their relative locations, sensors to use for activating different modes or sending waves, and so on.





Effects
-------

The main type of effect consist of nodes sending waves of light and color along light strips towards neighboring nodes, where the waves can carry on to other nodes.
Waves can have various intensity, hue/sat/lum (also negative), wavelength, travel distance/time, random noise, color gradients, and so on.  Multiple waves can travel along a strip in various directions
at the same time, the different waves are blended together.

Waves could leave the leds at zero after they pass, or they could leave them at some other intensity, which will be changed by the next wave.  (Or the base intensity might be based on the mode(s) the node is in?)

Waves can be used to create a lot of different effects (even global fade ins or outs).  If there are some ideas for effect types that are not possible to do with waves, those might be added later.

Single outputs, like led spotlights, could be treated like a led strip of length one (or some other length, if the spotlights are arranged in a row).


Node States
-----------

Nodes can have various states depending on their sensor readings and global or remote sensor readings received with communication from other nodes.
The state(s) a node is in determines what kind of effects it generates to attached led strips and other outputs.

A node need not be either in a state or not, it can also be partially in a state (e.g. a potentiometer could be used to place a node partially in a 'lit' state).

The degree a state is activated depends either directly or indirectly on one or more sensor inputs, with some multipliers.

Different states can also have different speeds or required activation that they activate or deactivate with (a kind of inertia), to provide some hysteresis and reduce quick flickering between states.

For more complicated behaviour, the activation degree of states in the current or neighboring nodes could be used as a sensor value to help determine the activation of a node (creating a neuron-like setup).


Modes
-----

Sometimes the general apperance of the whole network should be changed.  This could be done with a mode concept, where a mode enables states contained in it, and disables states contained within any previously active mode.
Some states might be independent of the mode.

One possibility is to allow multiple modes as once, although that might be more complications than necessary.

Modes could maybe change depending on signals (which would indicate some hierarchial state system might be better than a separate mode concept).

For example, during the evening a more orange toned lighting mode could be activated, there could be modes for work that needs a lot of light, for more muted lighting for general hanging around,
for some darker and slower night illumination, a party mode, and a few different looking general light modes.


Node types
----------

There are two different types of nodes:

* Led Node
  * Controls zero or more LED strips, LED lights, etc.  Also has two extra I/O pins.
  * Has locations for sensor modules such as PIR (motion detector) and sound.
  * Has builtin light level sensor.
  * Has location for a radio communication module.

* Remote Controller
  * Contains a character LCD screen and turn wheels (and possibly a color / color gradient preview LED) for configuring the network.  It allows:
     * Configuring the length of led strips connected to different nodes (needs to be done once after installation).  
     * Defining fixed signals for different nodes (e.g. height above floor, or x/y coordinate), which can be used in effects.
     * Defining the signals that different inputs on different nodes affect, and whether the signals are global or local to the node.
       Some inputs are pre-configured (e.g. light level sensors, etc), while others could be custom (e.g. door open microswitch, red alert/instant party button, etc).
     * Defining gradients.
     * Defining various named wave types.
     * Defining various wave generators (?).
     * Defining various node states
        * Connecting wave generators to the node state
        * Specifying the activation or deactivation effects of different signals (and state activations) from this node or neighboring nodes or global signals on this state.
        * Default lifetime of this state
        * Slowness / quickness of state change = change inertia
        * Whether this state should be available on all nodes, or only a selected subset (always non-active on other nodes)  [Is this needed?  Maybe not]
     * The default screen allows setting various selected global signals, and selecting the current mode(s).
  * Could also have a USB connection to computer, and some serial based protocol to provide updates to signal states, as well as communicate states from the Ledmess network.  The protocol could also contain commands for configuring LedMesh.
  * Might also contain support for bluetooth control (for control from android app)
  * Might support an ethernet connector as well.



Control and configuration application
-------------------------------------

To be written in Java, as that allows easy porting to both android and PC.

The software on the remote controller will probably be more limited, but it would be nice to be able to do all configuration from it if needed.


Builtin Signals
---------------

* Time of day
* Age of this state (0 = newborn, 100% = at the lifetime of the state).

