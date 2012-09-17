public class OSCGraphicsImage extends OSCGraphicsLayer {
	fun string
	file(string file)
	{
		osc_send.startMsg("/layer/"+name+"/file", "s");
		file => osc_send.addString;

		return file;
	}
}
