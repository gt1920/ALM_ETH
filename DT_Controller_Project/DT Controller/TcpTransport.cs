using System;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;

namespace DT_Controller
{
    /// <summary>
    /// TCP transport for ALM ETH devices.
    /// Frames are length-prefixed: [LEN_L][LEN_H][64-byte body]
    /// </summary>
    internal sealed class TcpTransport : ITransport, IDisposable
    {
        private TcpClient _client;
        private NetworkStream _stream;
        private CancellationTokenSource _cts;
        private readonly object _lock = new object();

        /// <summary>Fired on UI thread when a complete frame (64 bytes, no prefix) is received.</summary>
        public event Action<byte[]> FrameReceived;

        /// <summary>Fired when the connection is lost or closed.</summary>
        public event Action Disconnected;

        public bool IsConnected
        {
            get
            {
                lock (_lock)
                {
                    return _client != null && _client.Connected;
                }
            }
        }

        /// <summary>Connect to ALM ETH device.</summary>
        public async Task ConnectAsync(string host, int port)
        {
            Disconnect();

            var client = new TcpClient();
            client.NoDelay = true;
            client.ReceiveTimeout = 0;
            client.SendTimeout = 2000;

            await client.ConnectAsync(host, port);

            lock (_lock)
            {
                _client = client;
                _stream = client.GetStream();
                _cts = new CancellationTokenSource();
            }

            // Start receive loop
            Task.Run(() => ReceiveLoop(_cts.Token));
        }

        /// <summary>Send a 64-byte frame with 2-byte LE length prefix.</summary>
        public void Write(byte[] frame)
        {
            if (frame == null || frame.Length == 0)
                return;

            // Strip HID report ID if present (65 bytes with [0]=0x00)
            byte[] payload;
            if (frame.Length == 65 && frame[0] == 0x00)
            {
                payload = new byte[64];
                Array.Copy(frame, 1, payload, 0, 64);
            }
            else if (frame.Length == 64)
            {
                payload = frame;
            }
            else
            {
                return;
            }

            // Build length-prefixed packet: [LEN_L][LEN_H][payload]
            var packet = new byte[2 + payload.Length];
            packet[0] = (byte)(payload.Length & 0xFF);
            packet[1] = (byte)((payload.Length >> 8) & 0xFF);
            Array.Copy(payload, 0, packet, 2, payload.Length);

            lock (_lock)
            {
                if (_stream == null || !_client.Connected)
                    return;

                try
                {
                    _stream.Write(packet, 0, packet.Length);
                    _stream.Flush();
                }
                catch
                {
                    // Connection lost during write
                }
            }
        }

        public void Disconnect()
        {
            lock (_lock)
            {
                try { _cts?.Cancel(); } catch { }
                try { _stream?.Close(); } catch { }
                try { _client?.Close(); } catch { }
                _stream = null;
                _client = null;
                _cts = null;
            }
        }

        public void Dispose()
        {
            Disconnect();
        }

        private void ReceiveLoop(CancellationToken ct)
        {
            try
            {
                while (!ct.IsCancellationRequested)
                {
                    // Read 2-byte length prefix
                    var header = ReadExact(2, ct);
                    if (header == null)
                        break;

                    int frameLen = header[0] | (header[1] << 8);
                    if (frameLen <= 0 || frameLen > 256)
                        break; // protocol error

                    // Read frame body
                    var body = ReadExact(frameLen, ct);
                    if (body == null)
                        break;

                    FrameReceived?.Invoke(body);
                }
            }
            catch (OperationCanceledException)
            {
                // Normal shutdown
            }
            catch
            {
                // Connection error
            }

            Disconnected?.Invoke();
        }

        private byte[] ReadExact(int count, CancellationToken ct)
        {
            var buf = new byte[count];
            int offset = 0;

            while (offset < count)
            {
                ct.ThrowIfCancellationRequested();

                NetworkStream stream;
                lock (_lock)
                {
                    stream = _stream;
                }

                if (stream == null)
                    return null;

                int read;
                try
                {
                    read = stream.Read(buf, offset, count - offset);
                }
                catch
                {
                    return null;
                }

                if (read == 0)
                    return null; // Connection closed

                offset += read;
            }

            return buf;
        }
    }
}
