$(window).load(function () {
    main = new MainViewModel();
    ko.applyBindings(main);
});

var main;