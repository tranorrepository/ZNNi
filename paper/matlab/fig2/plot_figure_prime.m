function [  ] = plot_figure_prime( net, subpos, legentpos, fov )
% Create figure
figure1 = figure('Position', [100, 100, 650, 400]);
xmax = 0;
ymax = 0;

% Create axes
axes1 = axes('Parent',figure1,'YMinorGrid','on','YGrid','on',...
    'XMinorGrid','on',...
    'XGrid','on',...
    'Color',[0.931372549019608 0.915686274509804 0.884313725490196],...
    'YMinorTick','on',...
    'XMinorTick','on',...
    'XColor',[0 0 0],...
    'FontSize',12,...
    'AmbientLightColor',[0.8 0.8 0.8]);


box(axes1,'on');
hold(axes1,'on');
% Create ylabel
ylabel('Throughput','FontSize',20);

% Create xlabel
xlabel('Input size','FontSize',20);

[a, b] = read_data([net '.behir']);
plot(a + fov,b,'LineWidth',3.5,'Parent',axes1,'DisplayName','CPU (72 cores)');

if exist([net '.aws'], 'file')
    [a, b] = read_data([net '.aws']);
    plot(a + fov,b,'LineWidth',3.5,'Parent',axes1,'DisplayName','CPU (16 cores)');
end

if exist([net '.gpu.optimal'], 'file')
    [a, b] = read_data([net '.gpu.optimal']);
    xmax = max(xmax, max(a(:)));
    ymax = max(ymax, max(b(:)));
    plot(a + fov,b,'LineWidth',3.5,'Parent',axes1,'DisplayName','GPU');
end

legend1 = legend(axes1,'show');
set(legend1,...
    'Location', legentpos,...
    'Color',[1 1 1],'FontSize',16);


end

