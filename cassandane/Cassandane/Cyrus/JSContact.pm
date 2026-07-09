package Cassandane::Cyrus::JSContact;
use strict;
use warnings;

use base qw(Cassandane::Cyrus::TestCase);

sub new
{
    my $class = shift;
    return $class->SUPER::new({}, @_);
}

use Cassandane::Tiny::Loader;

1;