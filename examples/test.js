var example = require('./example');
var util = require('util');

var e = example.create();
var f = example.create('8000000000');

console.log('e = ' + e.toString());
console.log('f = ' + f.toString());

e.add(132);
console.log('e = ' + e.toString());

try {
	e.set(0x1111111111111111);
} catch (e) {
	console.log('got exception: ' + e.name + ' (' + e.message + ')');
}

e.set('0x1111111111111111');
console.log('e = ' + e.toString());

e.multiply(5);
console.log('e = ' + e.toString());

try {
	e.toString(33, 'fred');
} catch (e) {
	console.log('got exception: ' + e.name + ' (' + e.message + ')');
	console.log(util.inspect(e, false, null));
}

e.set(50000000);
f.set(22222222);
e.multiply(f.toString());
console.log('e = ' + e.toString());
console.log('f = ' + f.toString());

function
x()
{
	var n = example.create(32);
}

x();
