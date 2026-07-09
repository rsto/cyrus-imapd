package Cassandane::Cyrus::JSContact;
use strict;
use warnings;

use HTTP::Tiny;

use base qw(Cassandane::Cyrus::TestCase);
use Cassandane::Util::Log;

sub new {
    my ($class, @args) = @_;
    my $config = Cassandane::Config->default()->clone();

    $config->set(
        caldav_realm                => 'Cassandane',
        caldav_historical_age       => -1,
        conversations               => 'yes',
        httpmodules                 => 'convert',
        httpallowcompress           => 'no',
        jmap_nonstandard_extensions => 'yes',
        defaultdomain               => 'example.com'
    );

    my $self = $class->SUPER::new(
        {
            config   => $config,
            jmap     => 1,
            services => ['http'],
        },
        @args
    );

    $self->needs('component', 'jmap');
    return $self;
}

use Cassandane::Tiny::Loader;

1;
