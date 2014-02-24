ko.bindingHandlers.draggableElement = {
    init: function(element, valueAccessor, allBindings, viewModel, bindingContext) {
        // This will be called when the binding is first applied to an element
        // Set up any initial state, event handlers, etc. here
		var isDragging = false;
		var prevX = 0;
		var prevY = 0;
		
		var settings = ko.unwrap(valueAccessor());
		$(element).mousedown(function(e) {
			isDragging = true;
			//console.log('down', e.clientX, e.clientY);
			prevX = e.clientX;
			prevY = e.clientY;
		});
		
		$(element).parent().parent().mousemove(function(e) {
			if(isDragging) {
				//console.log('move', e.clientX, e.clientY);
				var deltaX = e.clientX - prevX;
				var deltaY = e.clientY - prevY;
				settings.x(settings.x() + deltaX);
				settings.y(settings.y() + deltaY);
				prevX = e.clientX;
				prevY = e.clientY;
			}
		});
		
		$(element).parent().parent().mouseup(function(e) {
			//console.log('up');
			isDragging = false;
		});
    },
    update: function(element, valueAccessor, allBindings, viewModel, bindingContext) {
        // This will be called once when the binding is first applied to an element,
        // and again whenever the associated observable changes value.
        // Update the DOM element based on the supplied values here.
    }
};