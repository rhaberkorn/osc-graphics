public class OSCGraphicsPort extends Chubgraph {
	inlet => blackhole;
	inlet => outlet;

	50::ms => dur poll_interval;

	fun void tick(float in) {}

	fun void
	poll()
	{
		inlet.last() => float prev;

		while (poll_interval => now)
			if (inlet.last() != prev)
				inlet.last() => prev => tick;
	}
	spork ~ poll();
}
