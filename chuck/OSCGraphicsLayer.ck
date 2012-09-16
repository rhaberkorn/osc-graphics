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
		for (0 => int i; i < 4; i++)
			geo[i] => osc_send.addInt;
	}

	class GeoPort extends OSCGraphicsPort {
		OSCGraphicsLayer @layer;

		int geo[];
		int index;

		fun void
		tick(float in)
		{
			in $ int => geo[index];
			geo => layer.geo;
		}
	}
	fun OSCGraphicsPort @
	getGeoPort(int geo[], int index)
	{
		GeoPort p;
		this @=> p.layer;
		geo @=> p.geo;
		index => p.index;

		return p;
	}
	fun int[]
	geo(int geo[])
	{
		if (geo == null)
			[0, 0, 0, 0] @=> geo;

		osc_send.startMsg("/layer/"+name+"/geo", "iiii");
		for (0 => int i; i < 4; i++)
			geo[i] => osc_send.addInt;

		return geo;
	}
	fun int[]
	geo()
	{
		return geo(null);
	}

	class AlphaPort extends OSCGraphicsPort {
		OSCGraphicsLayer @layer;

		fun void
		tick(float in)
		{
			in => layer.alpha;
		}
	}
	fun OSCGraphicsPort @
	getAlphaPort()
	{
		AlphaPort p;
		this @=> p.layer;

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
