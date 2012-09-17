public class OSCGraphicsVideo extends OSCGraphicsLayer {
	fun string
	url(string url)
	{
		osc_send.startMsg("/layer/"+name+"/url", "s");
		url => osc_send.addString;

		return url;
	}

	class RatePort extends OSCGraphicsPort {
		OSCGraphicsVideo @layer;

		fun void
		tick(float in)
		{
			in => layer.rate;
		}
	}
	fun OSCGraphicsPort @
	getRatePort()
	{
		RatePort p;
		this @=> p.layer;

		return p;
	}
	fun float
	rate(float rate)
	{
		osc_send.startMsg("/layer/"+name+"/rate", "f");
		rate => osc_send.addFloat;

		return rate;
	}

	class PositionPort extends OSCGraphicsPort {
		OSCGraphicsVideo @layer;

		fun void
		tick(float in)
		{
			in => layer.position;
		}
	}
	fun OSCGraphicsPort @
	getPositionPort()
	{
		PositionPort p;
		this @=> p.layer;

		return p;
	}
	fun float
	position(float position)
	{
		osc_send.startMsg("/layer/"+name+"/position", "f");
		position => osc_send.addFloat;

		return position;
	}

	fun int
	paused(int paused)
	{
		osc_send.startMsg("/layer/"+name+"/paused", "i");
		paused => osc_send.addInt;

		return paused;
	}
}
