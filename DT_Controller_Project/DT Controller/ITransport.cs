namespace DT_Controller
{
    internal interface ITransport
    {
        void Write(byte[] frame);
    }
}
