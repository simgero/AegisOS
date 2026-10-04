#!/usr/bin/perl
# Bounded synthetic Unix-socket fixture, run as the ordinary GNU UID 1000.
# No credentials, host listeners, network traffic, or changes to permissions.
use strict;
use warnings;
use Socket qw(AF_UNIX SOCK_STREAM sockaddr_un);
use IO::Select;
use Fcntl qw(O_WRONLY O_CREAT O_EXCL);

my ($mode, $label, $token, $peer_init, $peer_uid) = @ARGV;
die "mode/label/token required\n" unless defined($token)
    && $mode =~ /\A(?:serve|own|probe|stop)\z/
    && $label =~ /\A(?:alpha|beta)\z/ && $token =~ /\A[a-f0-9]{16}\z/;
die "ordinary runtime only\n" unless $> == 1000
    && ($ENV{HOME} // '') eq '/home/user'
    && ($ENV{XDG_RUNTIME_DIR} // '') eq '/run/user/1000';
my $stem = "aegis-ipc-$token";
my $ready = "/tmp/$stem-$label.ready";
my @own = ("/run/user/1000/$stem.sock",
           "/home/user/.aegis-proof/$stem-$label.sock",
           "\0$stem-common", "\0$stem-$label");

sub starttime {
    my ($pid) = @_;
    open(my $f, '<', "/proc/$pid/stat") or die "process stat: $!\n";
    my $v = <$f>; close $f;
    $v =~ s/^.*\) // or die "invalid stat\n";
    my @fields = split /\s+/, $v;
    return $fields[19];
}
sub connect_reply {
    my ($path) = @_;
    socket(my $s, AF_UNIX, SOCK_STREAM, 0) or die "socket: $!\n";
    connect($s, sockaddr_un($path)) or die "own connect: $!\n";
    my $readable = IO::Select->new($s);
    die "own response timeout\n" unless $readable->can_read(5);
    my $n = sysread($s, my $reply, 256);
    die "wrong own response\n" unless defined($n) && $reply eq "$token $label\n";
    close($s) or die "close: $!\n";
}

if ($mode eq 'serve') {
    umask 0077;
    die "fixture already exists\n" if -e $ready;
    my (@listeners, @paths);
    my $running = 1;
    $SIG{TERM} = $SIG{INT} = $SIG{ALRM} = sub { $running = 0; };
    $SIG{PIPE} = 'IGNORE';
    my $created_ready = 0;
    my $ok = eval {
        for my $path (@own) {
            die "socket path already exists\n" if substr($path, 0, 1) ne "\0" && -e $path;
            socket(my $s, AF_UNIX, SOCK_STREAM, 0) or die "socket: $!\n";
            bind($s, sockaddr_un($path)) or die "bind: $!\n";
            push @listeners, $s;
            push @paths, $path if substr($path, 0, 1) ne "\0";
            listen($s, 4) or die "listen: $!\n";
        }
        sysopen(my $f, $ready, O_WRONLY | O_CREAT | O_EXCL, 0600) or die "ready: $!\n";
        $created_ready = 1;
        print {$f} "$$ ", starttime($$), " $label $token\n" or die "ready write: $!\n";
        close($f) or die "ready close: $!\n";
        alarm 900;
        my $select = IO::Select->new(@listeners);
        while ($running) {
            for my $s ($select->can_read(1)) {
                accept(my $c, $s) or next;
                my $reply = "$token $label\n";
                my $n = syswrite($c, $reply);
                die "short fixture response\n" unless defined($n) && $n == length($reply);
                close $c;
            }
        }
        1;
    };
    my $error = $@;
    close $_ for @listeners;
    unlink $_ for @paths;
    unlink $ready if $created_ready;
    die $error unless $ok;
    print "SOCKET_FIXTURE_CLEANED $label\n";
    exit 0;
}

if ($mode eq 'stop') {
    open(my $f, '<', $ready) or die "ready: $!\n";
    my ($pid, $start, $owner, $nonce) = split /\s+/, <$f>; close $f;
    die "wrong fixture identity\n" unless $pid =~ /\A[0-9]+\z/
        && $owner eq $label && $nonce eq $token && starttime($pid) eq $start;
    open(my $args, '<', "/proc/$pid/cmdline") or die "cmdline: $!\n";
    local $/; my $cmdline = <$args>; close $args;
    my @args = split /\x00/, $cmdline;
    die "wrong fixture command\n" unless @args == 5
        && $args[0] eq 'perl' && $args[1] eq $0
        && $args[2] eq 'serve' && $args[3] eq $label && $args[4] eq $token;
    kill 'TERM', $pid or die "stop: $!\n";
    for (1 .. 50) { last unless -e $ready; select undef, undef, undef, 0.1; }
    die "fixture cleanup pending\n" if -e $ready;
    die "pathname socket remains\n" if -e $own[0] || -e $own[1];
    print "SOCKET_FIXTURE_STOPPED $label\n";
    exit 0;
}

connect_reply($_) for @own;
print "OWN_SOCKET_REPLIES=4 $label\n";
exit 0 if $mode eq 'own';
die "known peer init/uid required\n" unless defined($peer_uid)
    && $peer_init =~ /\A[1-9][0-9]*\z/ && $peer_uid eq ($label eq 'alpha' ? '11' : '10');
my $peer = $label eq 'alpha' ? 'beta' : 'alpha';
my @foreign = ("\0$stem-$peer",
    "/home/user/.aegis-proof/$stem-$peer.sock",
    "/data/misc_ce/$peer_uid/aegis/home/.aegis-proof/$stem-$peer.sock",
    "/proc/$peer_init/root/run/user/1000/$stem.sock",
    "/proc/$peer_init/root/home/user/.aegis-proof/$stem-$peer.sock");
for my $path (@foreign) {
    socket(my $s, AF_UNIX, SOCK_STREAM, 0) or die "socket: $!\n";
    die "unexpected peer connection\n" if connect($s, sockaddr_un($path));
    my $error = 0 + $!;
    die "unexpected connect error $error\n" unless $error == 1 || $error == 2 || $error == 13 || $error == 111;
    close $s;
}
connect_reply($_) for @own;
print "PEER_SOCKET_CONNECTIONS_DENIED=5 $label\n";
