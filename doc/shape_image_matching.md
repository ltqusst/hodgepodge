[Procrustes method](https://en.wikipedia.org/wiki/Procrustes_analysis) can match a set of shapes(represented by key points) into canonical position/scale/orientation.

  1. it's a necessary step before performing PCA on the shape data.*(to remove trival variations of shape, cause we want PCA focus on non-trival shape changes)*
  2. it has closed form solution.*(by define distance between shapes and minimize it over pos/scale/orientation using least-squres method)*

[Active shape model](https://en.wikipedia.org/wiki/Active_shape_model) introduced PCA model of  deformable shape.

  1. prior knowledge about  target shape is learned by PCA.

  2. initial shape is refined to final location in a iterative way, key points displacement in each step is found individually *(simply to nearest image edges)* and then map to shape parameter space spanned by PCA's eignvectors.

[Active appearance model](https://www.cs.cmu.edu/~efros/courses/LBMV07/Papers/cootes-eccv-98.pdf) introduced PCA model of face-images:

  1. the face images are pre-aligned according to pre-aligned shape/landmarks*(using Procrustes method)*. so the pos/scale/orientation of the image is also normalized.
  2. further more, a shape-normalized face image is generated by warping *(using triangulation algorithm)* according to landmarks, the result face image only represents appearence with no landmark displacement.
  3. another factor they don't want PCA to model is the light condition variation, so every pos/scale/orientation aligned raw image is prescaled by remove mean and normalize variances*(the way it performes this step is recursively)*.
  4. with these two PCA model, every sample becomes a point in this joint shape&image parameter space.

[Kanade–Lucas–Tomasi (KLT) feature tracker](https://en.wikipedia.org/wiki/Kanade%E2%80%93Lucas%E2%80%93Tomasi_feature_tracker) can be generalized to track complex transformations, w/o the need of shape concept because the transform is rigid/non-deformable.

