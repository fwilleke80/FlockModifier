# FlockModifier
The Flock Modifier is a modifier object for the good old standard particle system in Cinema 4D. It allows to easily apply the behavior of flocks, swarms or schools to the particles.

The modifier uses a distributed behavioral model, as described by Craig Reynolds on his 1987 SIGGRAPH paper "Flocks, Herds, and Schools: A Distributed Behavioral Model".

Several of the classic "Reynolds Rules" are implemented. Each particle will...
...keep a minimum distance to its neighbors
...try to stay in the center of the flock
...chase a target
...match the velocity and travel direction of its flockmates
...obey a certain minimum / maximum speed limit
...avoid collision with geometry
...fly level / avoid steep rising or falling
...apply a certain randomness to its movement
