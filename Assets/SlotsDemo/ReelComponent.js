ReelComponent = class ReelComponent extends o2.Component
{
    constructor()
     {
        super();

        this.imagesContainer = new o2.LinkRefActor();

        this.images = [ new o2.AssetRefImageAsset() ];
        this.blurredImages = [ new o2.AssetRefImageAsset() ];
    
        this.imagesDistance = 100; 
        this.rotationSpeed = 100;
    
        this.isRotating = true;
        this.isClipping = true;    
        this.isBlurred = true;   

        this.disableExtendedSymbols = false; 

        this._createdImages = []
        this._rotatingOffset = 0;
    }

    OnStart() 
    {
        this.CreateImages();
    }

    Update(dt)
    {
        if (!this.isRotating)
            return;
    
        this._rotatingOffset += this.rotationSpeed*dt;

        this.UpdateImagesLayout();
    }

    UpdateImagesLayout()
    {
        let allImagesHeight = this.imagesDistance*this._createdImages.length;
    
        for (let i = 0; i < this._createdImages.length; i++)
        {
            let image = this._createdImages[i];    
            let imageOffset = (i*this.imagesDistance + this._rotatingOffset)%allImagesHeight - allImagesHeight/2 + this.imagesDistance/2;
            let imageSize = image.GetImageAsset().Get().GetSize();
            let layoutSize = this.isBlurred ? new Vec2(imageSize.x*2, imageSize.y*2) : imageSize;
            image.GetTransform().Set(o2.WidgetLayout.Based("Center", layoutSize, new Vec2(0, imageOffset)));
    
            if (this.isClipping)
            {
                image.SetEnabled(imageOffset >= -image.GetTransform().GetHeight()/2 - this.imagesDistance*2.5 &&
                                 imageOffset <= image.GetTransform().GetHeight()/2 + this.imagesDistance*2.5);
            }
        }
    }

    CreateImages()
    {
        if (this.imagesContainer.Get() == null)
            return;
    
        let imagesSource = this.isBlurred ? this.blurredImages : this.images;
    
        if (imagesSource.length == 0)
            return;
    
        let requiredImagesByHeightCount = Math.ceil(this._actor.GetTransform().GetHeight() / this.imagesDistance);
        let requiredImages = Math.max(requiredImagesByHeightCount, imagesSource.length);
    
        if (this.disableExtendedSymbols)
            requiredImages = Math.min(requiredImages, 3);
    
        for (let i = 0; i < requiredImages; i++)
        {
            let imageAsset = imagesSource[i%imagesSource.length];
            let newImage = new o2.Image();
            let imageSize = imageAsset.Get().GetSize();
            
            newImage.SetName(imageAsset.Get().GetPath().replace(/^.*[\\\/]/, ''));
            newImage.SetImageAsset(imageAsset);
            newImage.GetTransform().SetSize(this.isBlurred ? new Vec2(imageSize.x*2, imageSize.y*2) : imageSize);
            newImage.GetTransform().SetPivot(new Vec2(0.5, 0.5));
            newImage.SetLayer("Reels");
            newImage.SetParent(this.imagesContainer.Get(), false);
    
            this._createdImages.push(newImage);
        }

        this.UpdateImagesLayout();
    }
}