function [  ] = plot_network_memory( net )
% Create figure
figure1 = figure;

% Create axes
axes1 = axes('Parent',figure1,'YMinorGrid','on','YGrid','on',...
    'XMinorGrid','on',...
    'XGrid','on',...
    'Color',[0.931372549019608 0.915686274509804 0.884313725490196],...
    'YMinorTick','on',...
    'XMinorTick','on',...
    'XColor',[0 0 0],...
    'FontSize',16,...
    'AmbientLightColor',[0.8 0.8 0.8]);


box(axes1,'on');
hold(axes1,'on');
% Create ylabel
ylabel('Theoretical speedup','FontSize',20);
%ylim([-100 150000]);

% Create xlabel
xlabel('Memory required (GB)','FontSize',20);
xlim([0 512]);

z = net.sparse_cost(net,1,1);
x = z.fft.flops;


for bp = 0:5
    b = 2^bp;
    int = 8:8:(1000/(b^(1/3)));
    z = net.dense_cost(net,b,int);
    
    label = sprintf('S = %d', b);
    
    plot(z.fft.gb, x./z.fft.flops, 'LineWidth',2.5,'Parent',axes1,'DisplayName',label);
end


% for bp = 0:0
%     b = 2^bp;
%     int = 8:8:(1000/(b^(1/3)));
%     z = net.dense_cost(net,b,int);
%     
%     label = sprintf('Cached FFTs');
%     
%     plot(z.cfft.gb, x./z.cfft.flops, 'LineWidth',2.5,'Parent',axes1,'DisplayName',label);
% end


legend1 = legend(axes1,'show');
set(legend1,...
    'Location', 'NorthWest',...
    'Color',[1 1 1],'FontSize',14);


end

