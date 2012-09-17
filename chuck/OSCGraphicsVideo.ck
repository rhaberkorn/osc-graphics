public class OSCGraphicsVideo extends OSCGraphicsLayer {
	fun string
	url(string url)
	{
		osc_send.startMsg("/layer/"+name+"/url", "s");
		url => osc_send.addString;

		return url;
	}
}
