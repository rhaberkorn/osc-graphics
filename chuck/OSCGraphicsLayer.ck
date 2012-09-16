public class OSCGraphicsLayer {
	OscSend @osc_send;
	string name;

	fun void
	init(OscSend @osc_send, string type, string osc_types,
	     int pos, string name, int geo[]) /* pseudo-constructor */
	{
		if (geo == null)
			[0, 0, 0, 0] @=> geo;

		osc_send @=> this.osc_send;
		name => this.name;

		osc_send.startMsg("/layer/new/"+type, "isiiii"+osc_types);
		pos => osc_send.addInt;
		name => osc_send.addString;
		for (0 => int i; i < geo.cap(); i++)
			geo[i] => osc_send.addInt;
	}

	fun int[]
	geo(int geo[])
	{
		if (geo == null)
			[0, 0, 0, 0] @=> geo;

		osc_send.startMsg("/layer/"+name+"/geo", "iiii");
		for (0 => int i; i < geo.cap(); i++)
			geo[i] => osc_send.addInt;

		return geo;
	}
	fun int[]
	geo()
	{
		return geo(null);
	}

	class AlphaPort extends Chubgraph {
		inlet => blackhole;
		inlet => outlet;

		fun void
		poll(OSCGraphicsLayer @layer)
		{
			inlet.last() => float prev;

			while (50::ms => now)
				if (inlet.last() != prev)
					inlet.last() => layer.alpha => prev;
		}
	}
	fun UGen @
	getAlphaPort()
	{
		AlphaPort p;
		spork ~ p.poll(this);
		return p;
	}
	fun float
	alpha(float opacity)
	{
		osc_send.startMsg("/layer/"+name+"/alpha", "f");
		opacity => osc_send.addFloat;
		return opacity;
	}

	fun void
	delete()
	{
		osc_send.startMsg("/layer/"+name+"/delete", "");
		"" => name;
	}
}
