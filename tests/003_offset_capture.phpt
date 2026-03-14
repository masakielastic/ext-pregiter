--TEST--
preg_iter_ex supports PREG_OFFSET_CAPTURE
--EXTENSIONS--
pregiter
--FILE--
<?php
$it = preg_iter_ex('/(?P<word>[a-z]+)/', 'abc 123 def', PREG_OFFSET_CAPTURE);
foreach ($it as $match) {
    var_dump($match);
}
?>
--EXPECT--
array(3) {
  [0]=>
  array(2) {
    [0]=>
    string(3) "abc"
    [1]=>
    int(0)
  }
  ["word"]=>
  array(2) {
    [0]=>
    string(3) "abc"
    [1]=>
    int(0)
  }
  [1]=>
  array(2) {
    [0]=>
    string(3) "abc"
    [1]=>
    int(0)
  }
}
array(3) {
  [0]=>
  array(2) {
    [0]=>
    string(3) "def"
    [1]=>
    int(8)
  }
  ["word"]=>
  array(2) {
    [0]=>
    string(3) "def"
    [1]=>
    int(8)
  }
  [1]=>
  array(2) {
    [0]=>
    string(3) "def"
    [1]=>
    int(8)
  }
}
