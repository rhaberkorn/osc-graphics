public class OSCGraphicsPort extends Chubgraph {
	inlet => blackhole;
	inlet => outlet;

	second/20 => dur poll_interval;

	fun void tick(float in) {} /* virtual */
	fun void
	tick(float in, float prev) /* virtual */
	{
		in => tick;
	}

	fun void
	poll()
	{
		inlet.last() => float prev;

		while (poll_interval => now)
			if (inlet.last() != prev) {
				tick(inlet.last(), prev);
				inlet.last() => prev;
			}
	}
	spork ~ poll();
}
