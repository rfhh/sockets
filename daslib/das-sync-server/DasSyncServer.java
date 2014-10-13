import java.io.*;
import java.net.*;
import java.util.*;

public class DasSyncServer extends Thread {
    final static int    DAS_SYNC_PORT = 3456;
    ServerSocket        listen_socket;
    DasSyncDatabase     database = new DasSyncDatabase();
    final static int    s = 1000;
    static int          connect_timeout = 150 * s;

    static void fail(Exception e, String msg) {
        System.err.println(msg + ": " + e);
        System.exit(33);
    }

    public DasSyncServer(int port) {
        try {
            listen_socket = new ServerSocket(port);

        } catch (IOException e) {
            fail(e, "das sync server: create listen socket fail");
        }

        System.out.println("das sync server runs, listen to port " + port);
        this.start();
    }

    public void run() {
        new DasSyncWatchdog(database, connect_timeout);

        try {
            while (true) {
                Socket client_socket = listen_socket.accept();
                Connection c = new Connection(client_socket, database,
                                              connect_timeout);
            }
        } catch (IOException e) {
            fail(e, "das sync server: accept fail");
        }
    }

    public static void main(String[] args) {
        int     port = DAS_SYNC_PORT;

        for (int i = 0; i < args.length; i++) {
            if (false) {
            } else if (args[i].compareTo("-port") == 0) {
                try {
                    port = Integer.parseInt(args[++i]);
                } catch (ArrayIndexOutOfBoundsException e) {
                    fail(e, "das sync server: -port requires a number ");
                } catch (NumberFormatException e) {
                    fail(e, "das sync server: port number " + args[i] +
                         " should be an int");
                }
            } else if (args[i].compareTo("-timeout") == 0) {
                try {
                    connect_timeout = Integer.parseInt(args[++i]) * s;
                } catch (ArrayIndexOutOfBoundsException e) {
                    fail(e, "das sync server: -port requires a number ");
                } catch (NumberFormatException e) {
                    fail(e, "das sync server: port number " + args[i] +
                         " should be an int");
                }
            }
        }

        new DasSyncServer(port);
    }
}


class DasSyncWatchdog extends Thread {
    static final long   s = 1000;
    static final long   WATCHDOG_INTERVAL = 5 * s;
    DasSyncDatabase     database;
    int                 connect_timeout;

    public DasSyncWatchdog(DasSyncDatabase database, int connect_timeout) {
        this.database = database;
        this.connect_timeout = connect_timeout;
        this.start();
    }

    public synchronized void run() {
        while (true) {
            try {
                wait(WATCHDOG_INTERVAL);
                database.check_timeouts(connect_timeout);
            } catch (InterruptedException e) {
                System.err.println("Watchdog wait fails: " + e);
            }
        }
    }
}


class DasSyncDatabase extends Hashtable {

    public synchronized DasSyncRun lookup(String key, int hosts) {
        DasSyncRun      p;

        p = (DasSyncRun)get(key);
        if (p == null) {
            p = new DasSyncRun(key, hosts, this);
            put(key, p);
        }
        return p;
    }

    public synchronized void check_timeouts(int connect_timeout) {
        DasSyncRun      p;
        long            now = System.currentTimeMillis();

        for (Enumeration e = elements(); e.hasMoreElements(); ) {
            p = (DasSyncRun)e.nextElement();
            if (now - p.creation_time > connect_timeout) {
                System.err.println("Clean-up partial service " + p.id);
                remove(p.id);
            }
        }
    }
}


class Inet_info {
    int                 port;
    String              hostname;
    boolean             empty;
    int                 call_attempt;

    public Inet_info() {
        empty = true;
        call_attempt = 0;
    }
}


class DasSyncRun {
    String              id;
    int                 n_clusters;
    int                 registered;
    int                 retrieved;
    Inet_info[]         cluster_inet;
    DasSyncDatabase     database;
    long                creation_time;

    public DasSyncRun(String id, int n, DasSyncDatabase database) {
        cluster_inet = new Inet_info[n];
        n_clusters = n;
        retrieved = 0;
        this.id = id;
        this.database = database;
        creation_time = System.currentTimeMillis();
        for (int i = 0; i < n; i++) {
            cluster_inet[i] = new Inet_info();
        }
    }

    public synchronized void store(int i, Inet_info info)
            throws IndexOutOfBoundsException {
        if (i >= n_clusters || i < 0) {
            throw new IndexOutOfBoundsException("Host " + i + " exceeds max " +
                                                n_clusters);
        }

        cluster_inet[i] = info;
        cluster_inet[i].call_attempt++;

        if (cluster_inet[i].call_attempt == 1) {
            registered++;

            if (registered == n_clusters) {
                notifyAll();
            }
        }
    }

    public synchronized Inet_info[]
    retrieve_all(int host_nr, int call_attempt, int connect_timeout)
            throws InterruptedException {
        while (registered < n_clusters) {
            wait(connect_timeout);
            if (registered < n_clusters) {
                throw new InterruptedException("Waiting thread " + host_nr +
                                               " hits timeout " + id);
            }
            if (call_attempt != cluster_inet[host_nr].call_attempt) {
                throw new InterruptedException("Waiting thread " + host_nr +
                                               " superseeded " + id);
            }
        }

        retrieved++;
        if (retrieved == n_clusters) {
            Date now = new Date();
            database.remove(id);
System.out.println("Ack " + id);
System.out.println(now.toLocaleString() + ": # live threads = " + Thread.currentThread().getThreadGroup().activeCount() + " # database keys = " + database.size());
        }

        return cluster_inet;
    }
}


class Connection extends Thread {
    final int           MAX_INET_NAME = 15;
    final int           MAX_ID_LEN    = 1024;
    protected Socket    client;
    protected DataInputStream   in;
    protected PrintStream       out;
    DasSyncDatabase     database;
    int                 connect_timeout;

    public Connection(Socket client_socket, DasSyncDatabase database,
                      int connect_timeout) {
        client = client_socket;

        try {
            in  = new DataInputStream(client.getInputStream());
            out = new PrintStream(client.getOutputStream());

        } catch (IOException e) {
            try {
                client.close();
            } catch (IOException eclose) {
            }
            System.err.println("das sync server: open streams fail");
            return;
        }

        this.database = database;
        this.start();
        this.connect_timeout = connect_timeout;
    }


    String read_word(DataInputStream in)
            throws IOException {
        final int MAX_STRLEN = 1024;
        StringBuffer    v = new StringBuffer();
        char            b;

        while (true) {
            b = (char)in.readByte();
            if (Character.isWhitespace(b)) {
                break;
            }
            v.append(b);
        }

        return v.toString();
    }


    int read_int(DataInputStream in)
            throws NumberFormatException, IOException {
        return Integer.parseInt(read_word(in));
    }


    public void run() {
        Inet_info       info = new Inet_info();
        Inet_info[]     all_info;
        int             host_nr;
        int             total_hosts;
        String          id;
        DasSyncRun      p;

        try {
            id = read_word(in);
            host_nr = read_int(in);
            total_hosts = read_int(in);
            info.port = read_int(in);
            info.hostname = read_word(in);
System.out.println("Client: id " + id + " host " + host_nr + " #hosts " + total_hosts + " port " + info.port + " inet " + info.hostname);
        } catch (NumberFormatException e) {
            System.err.println("Not an integer: " + e);
            out.close();
            return;
        } catch (IOException e) {
            System.err.println("read_word fails: " + e);
            out.close();
            return;
        }

        p = database.lookup(id, total_hosts);
        try {
            p.store(host_nr, info);
        } catch (IndexOutOfBoundsException e) {
            System.err.println("Key " + id + ": " + e);
            out.close();
            return;
        }

                                        // Wait until all clients connected
        try {
            all_info = p.retrieve_all(host_nr, info.call_attempt,
                                      connect_timeout);

            for (int i = 0; i < total_hosts; i++) {
                out.println(i + " " + all_info[i].port + " " +
                            all_info[i].hostname);
            }
        } catch (InterruptedException e) {
            System.err.println("das sync server: " + e + " wait/retrieve failed");
        }

        out.close();
    }
}
