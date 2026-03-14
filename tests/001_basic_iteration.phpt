--TEST--
preg_iter_ex iterates simple matches
--EXTENSIONS--
pregiter
--FILE--
<?php
$it = preg_iter_ex('/[a-z]+/', 'abc 123 def');
foreach ($it as $i => $match) {
    var_dump($i, $match);
}
?>
--EXPECT--
int(0)
array(1) {
  [0]=>
  string(3) "abc"
}
int(1)
array(1) {
  [0]=>
  string(3) "def"
}
